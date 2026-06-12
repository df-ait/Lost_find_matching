//匹配评分与精确/模糊寻物
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "match.h"
#include "item_mgr.h"

//在needle当中寻找子串hay，用于名称地点的部分匹配，以及描述，特诊
static int str_icontains(const char *hay, const char *needle)
{
    if (!hay || !needle || !needle[0]) return 0;
    size_t nlen = strlen(needle);
    for (const char *p = hay; *p; p++) {
        size_t i = 0;
        while (i < nlen && p[i] &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i]))
            i++;
        if (i == nlen) return 1;
    }
    return 0;
}

//比的是 Item.foundTime交到招领处的时间和 LostReport 丢失时间区间
//失主没填写时间区间，就是0分
//foundTime落在失主说的丢失区间内，那么就是25分
//如果在区间外，和区间边界差距超过1H，就相应的减分数
static double time_score(const Item *item, const LostReport *report)
{
    if (report->lostTimeStart == 0 && report->lostTimeEnd == 0)
        return 0.0;
    if (report->lostTimeEnd == 0)
        return 0.0;

    if (item->foundTime >= report->lostTimeStart && item->foundTime <= report->lostTimeEnd)
        return 25.0;

    time_t mid;
    if (report->lostTimeStart > 0 && report->lostTimeEnd > report->lostTimeStart)
        mid = (report->lostTimeStart + report->lostTimeEnd) / 2;
    else
        mid = report->lostTimeStart;
    //diff_hours = |foundTime - mid|
    double diff_hours = fabs(difftime(item->foundTime, mid)) / 3600.0;
    if (diff_hours >= 48.0) return 0.0;
    return 25.0 * (1.0 - diff_hours / 48.0);
}

double calc_match_score(const Item *item, const LostReport *report)
{
    double score = 0.0;
    if (!item || !report) return 0.0;

    if (report->itemName[0]) {
        if (strcmp(item->name, report->itemName) == 0)
            score += 40.0;
        else if (str_icontains(item->name, report->itemName) ||
                 str_icontains(report->itemName, item->name))
            score += 20.0;
    }

    if (report->category[0] && strcmp(item->category, report->category) == 0)
        score += 30.0;

    score += time_score(item, report);

    if (report->lostLocation[0] && strcmp(report->lostLocation, "未知") != 0) {
        if (strcmp(item->location, report->lostLocation) == 0)
            score += 25.0;
        else if (str_icontains(item->location, report->lostLocation) ||
                 str_icontains(report->lostLocation, item->location))
            score += 10.0;
    }

    if (report->description[0] &&
        str_icontains(item->description, report->description))
        score += 15.0;

    return score;
}

static const char *utf8_next(const char *p)
{
    if (!p || !*p) return p;
    unsigned char c = (unsigned char)*p;
    if (c < 0x80) return p + 1;
    if ((c & 0xE0) == 0xC0) return p + 2;
    if ((c & 0xF0) == 0xE0) return p + 3;
    if ((c & 0xF8) == 0xF0) return p + 4;
    return p + 1;
}

static int is_secret_delim(char c)
{
    return c == ' ' || c == '\t' || c == ','
        || c == '，' || c == '、' || c == ';' || c == '；'
        || c == ':' || c == '：';
}

/* secret 中任意连续 min_chars 个字符（按 UTF-8）出现在 answer 里即算命中 */
static int share_char_window(const char *secret, const char *answer, int min_chars)
{
    if (!secret || !answer || min_chars <= 0) return 0;

    for (const char *p = secret; *p; p = utf8_next(p)) {
        const char *end = p;
        int n = 0;
        while (*end && n < min_chars) {
            end = utf8_next(end);
            n++;
        }
        if (n < min_chars) break;

        char chunk[32];
        int blen = (int)(end - p);
        if (blen >= (int)sizeof(chunk)) blen = (int)sizeof(chunk) - 1;
        memcpy(chunk, p, (size_t)blen);
        chunk[blen] = '\0';
        if (str_icontains(answer, chunk))
            return 1;
    }
    return 0;
}

int verify_secret(const Item *item, const char *answer)
{
    if (!item || !answer || !answer[0]) return 0;
    const char *sec = item->secretFeatures;
    if (!sec[0]) return 0;

    /* 1. 双向子串：登记特征与答案互相包含即可 */
    if (str_icontains(sec, answer) || str_icontains(answer, sec))
        return 1;

    /* 2. 分词：按空格与中英文标点拆分，任一词（≥2 字节）命中即过 */
    char buf[SECRET_LEN];
    strncpy(buf, sec, SECRET_LEN - 1);
    buf[SECRET_LEN - 1] = '\0';

    char *p = buf;
    while (*p) {
        while (*p && is_secret_delim(*p)) {
            *p = '\0';
            p++;
        }
        if (!*p) break;

        char *start = p;
        while (*p && !is_secret_delim(*p)) p++;
        if (*p) {
            *p = '\0';
            p++;
        }
        if (start[0] && strlen(start) >= 2) {
            if (str_icontains(answer, start) || str_icontains(start, answer))
                return 1;
        }
    }

    /* 3. 滑动窗口：登记特征里任意连续 2 个汉字/字符出现在答案中即可
     *    例：登记「内部有一张学生证」，答案「里面有学生证」→ 共享「学生证」 */
    if (share_char_window(sec, answer, 2))
        return 1;

    return 0;
}

//地点匹配
static int location_match(const Item *item, const LostReport *report)
{
    if (!report->lostLocation[0] || strcmp(report->lostLocation, "未知") == 0)
        return 1;
    return strcmp(item->location, report->lostLocation) == 0 ||
           str_icontains(item->location, report->lostLocation) ||
           str_icontains(report->lostLocation, item->location);
}

//计算好分数以后放入大顶堆当中
static void push_candidate(const Item *item, const LostReport *report, MaxHeap *heap)
{
    if (item->status != ITEM_IN_STORAGE) return;
    MatchResult r;
    memset(&r, 0, sizeof(r));
    strncpy(r.itemId, item->itemId, ID_LEN - 1);
    r.score = calc_match_score(item, report);
    r.item = *item;
    heap_push(heap, &r);
}

//精确寻物，失主记得大概丢失时间和地点
int match_precise(ItemStore *store, const LostReport *report, MaxHeap *heap, int top_k)
{
    if (!store || !report || !heap) return 0;

    heap_clear(heap);

    time_t t0 = report->lostTimeStart;
    time_t t1 = report->lostTimeEnd;
    if (t0 == 0 && t1 == 0) return 0;
    if (t1 == 0) t1 = t0;

    ItemList *candidates = list_create();
    if (!candidates) return 0;

    //按照t0,t1范围查找物品
    bst_range_query(store->by_time, t0, t1, candidates);

    for (ListNode *p = candidates->head; p; p = p->next) {
        if (location_match(&p->item, report)){
            //地点能够匹配成功，那么计算分数然后放入堆
            push_candidate(&p->item, report, heap);
        }
            
    }

    list_destroy(candidates);
    return heap_size(heap);
}

//模糊匹配，记不清时间/地点时，按名称、类别、描述等打分，全库找最像的
static void fuzzy_collect(const Item *item, void *ctx)
{
    FuzzyCtx *fc = (FuzzyCtx *)ctx;
    if (item->status != ITEM_IN_STORAGE) return;
    double s = calc_match_score(item, fc->report);
    if (s < 10.0) return;
    push_candidate(item, fc->report, fc->heap);
}

int match_fuzzy(ItemStore *store, const LostReport *report, MaxHeap *heap, int top_k)
{
    if (!store || !report || !heap) return 0;

    heap_clear(heap);
    FuzzyCtx ctx = { report, heap };
    //遍历库中所有物品，为所有物品进行模糊匹配
    list_foreach(store->all_items, fuzzy_collect, &ctx);
    return heap_size(heap);
}

static const char *status_str(ItemStatus s)
{
    switch (s) {
    case ITEM_IN_STORAGE: return "在库";
    case ITEM_CLAIMED:    return "已认领";
    case ITEM_ARCHIVED:   return "已归档";
    default:              return "未知";
    }
}

//把得分最高的K条打成表格
void print_match_results(const MatchResult *results, int count)
{
    if (!results || count <= 0) {
        printf("!!（无匹配结果）\n");
        return;
    }
    printf("\n  %-14s %-8s %-12s %-18s %-10s %s\n",
           "编号", "得分", "名称", "类别", "状态", "地点");
    printf("  %s\n", "----------------------------------------------------------------");
    for (int i = 0; i < count; i++) {
        printf("  %-14s %6.1f   %-12s %-18s %-10s %s\n",
               results[i].itemId,
               results[i].score,
               results[i].item.name,
               results[i].item.category,
               status_str(results[i].item.status),
               results[i].item.location);
        printf("----描述: %s\n", results[i].item.description);
    }
}
