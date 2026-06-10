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

//登记时是否写了保密特征
int item_has_secret(const Item *item)
{
    if (!item) return 0;
    const char *p = item->secretFeatures;
    while (*p == ' ' || *p == '\t')
        p++;
    return *p != '\0';
}

int verify_secret(const Item *item, const char *answer)
{
    if (!item || !answer || !answer[0]) return 0;
    if (str_icontains(item->secretFeatures, answer))
        return 1;

    //关键词命中，按空格/逗号拆分secret，统计命中数，先拷贝一份再进行该操作
    char buf[SECRET_LEN];
    strncpy(buf, item->secretFeatures, SECRET_LEN - 1);
    buf[SECRET_LEN - 1] = '\0';

    //类似于分词器
    int hits = 0;
    int tokens = 0;
    char *p = buf;
    while (*p){
        while (*p == ' ' || *p == ',') {
            *p = '\0';
            p++;
        }
        if (!*p) break;
        char *start = p;//将每个词的开头赋值给start
        while (*p && *p != ' ' && *p != ',') p++;
        if (*p){ 
            *p = '\0'; 
            p++; 
        }
        //存在这个词，才继续匹配
        if (start[0]){
            tokens++;//这个是分出来的秘密信息的词组数量
            if (str_icontains(answer, start))
                hits++;
        }
    }
    if (tokens == 0) return 0;
    return hits >= 1 && (hits * 100 / tokens) >= 50;//命中 50%以上才返回1
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
