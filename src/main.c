#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "console_utf8.h"
#include "types.h"
#include "item_mgr.h"
#include "claim_mgr.h"
#include "match.h"
#include "heap.h"

static ItemStore  *g_items  = NULL;
static ClaimStore *g_claims = NULL;

//去掉字符串末尾的换行符
static void trim_newline(char *s)
{
    if (!s) return;
    size_t n = strlen(s);
    //从最后一个字符倒着找\n,\r，替换为结束符\0
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

//基于fgets()实现带提示读取一行数据
static void read_line(const char *prompt, char *buf, int size)
{
    //prompt非空就输入提示词
    if (prompt) printf("%s", prompt);
    if (!fgets(buf, size, stdin)) {
        //fget失败就返回空串
        buf[0] = '\0';
        return;
    }
    trim_newline(buf);
}

//把输入的时间字符串变成time_t格式
static int parse_datetime(const char *str, time_t *out)
{
    if (!str || !out || !str[0]) return -1;
    struct tm t;
    memset(&t, 0, sizeof(t));
    int y, mo, d, h, mi;
    //从字符串中扫描这些数据填入上面的int
    if (sscanf(str, "%d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) != 5)
        return -1;
    t.tm_year = y - 1900;
    t.tm_mon  = mo - 1;//从0开始访问月份
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min  = mi;
    t.tm_sec  = 0;//固定0秒
    t.tm_isdst = -1;//夏令时判断
    //把填好的struct tm转成 time_t 时间戳（从 1970-01-01 起的秒数）
    *out = mktime(&t);
    //这里是将-1转换为time_t的形式才进行比较的
    return (*out == (time_t)-1) ? -1 : 0;
}

static void print_menu()
{
    printf("\n========== 主菜单 ==========\n");
    printf("1. 登记拾物（管理员）\n");
    printf("2. 精确寻物（时间+地点）\n");
    printf("3. 模糊寻物（名称/类别为主）\n");
    printf("4. 提交认领申请\n");
    printf("5. 审核认领（管理员）\n");
    printf("6. 按编号查询物品\n");
    printf("7. 浏览在库物品列表\n");
    printf("0. 退出\n");
    printf("============================\n");
    printf("请选择: ");
}

//有人捡到失物，交到系统处进行登记
static void menu_register()
{
    Item item;
    memset(&item, 0, sizeof(item));

    char timebuf[64];
    g_items->id_counter++;
    //在这里就生成物品ID
    generate_item_id(item.itemId, g_items->id_counter);

    read_line("--物品名称: ", item.name, NAME_LEN);
    read_line("--物品类别(如钱包/钥匙/手机): ", item.category, CATEGORY_LEN);
    read_line("--交到招领处时间 (YYYY-MM-DD HH:MM): ", timebuf, sizeof(timebuf));
    if (parse_datetime(timebuf, &item.foundTime) != 0) {
        printf("!!时间格式错误。\n");
        return;
    }
    read_line("--拾取/交到地点: ", item.location, LOC_LEN);
    read_line("--外观描述(对外可见): ", item.description, DESC_LEN);
    read_line("--独有特征(保密，认领验证用): ", item.secretFeatures, SECRET_LEN);

    item.status = ITEM_IN_STORAGE;
    item.registerTime = time(NULL);

    if (store_register_item(g_items, &item) == 0)
        printf("\n--登记成功！物品编号: %s\n", item.itemId);
    else
        printf("\n--登记失败，请重试。\n");
}

//失主来认领物品的时候需要输入的
static void fill_lost_report_base(LostReport *rpt)
{
    memset(rpt, 0, sizeof(*rpt));
    read_line("--物品名称(可回车跳过): ", rpt->itemName, NAME_LEN);
    read_line("--物品类别(可回车跳过): ", rpt->category, CATEGORY_LEN);
    read_line("--失主姓名: ", rpt->ownerName, NAME_LEN);
    read_line("--联系电话: ", rpt->phone, PHONE_LEN);
    read_line("--补充描述(可回车跳过): ", rpt->description, DESC_LEN);
}

//精准寻物，用户只需要填写丢失时间点+地点，然后自动生成四小时范围区间
static void menu_search_precise(void)
{
    LostReport rpt;
    memset(&rpt, 0, sizeof(rpt));
    //输入失物信息
    fill_lost_report_base(&rpt);

    char timebuf[64];
    read_line("--丢失时间 (YYYY-MM-DD HH:MM): ", timebuf, sizeof(timebuf));
    time_t t;
    //调整时间格式
    if (parse_datetime(timebuf, &t) != 0) {
        printf("!!时间格式错误。\n");
        return;
    }
    rpt.lostTimeStart = t - TIME_WINDOW_SEC;
    rpt.lostTimeEnd   = t + TIME_WINDOW_SEC;

    read_line("--丢失地点: ", rpt.lostLocation, LOC_LEN);

    //创建大根堆，存放算出分数的物品
    MaxHeap *heap = heap_create(MAX_TOP_K * 8);
    if (!heap) return;

    //精准寻物逻辑
    int n = match_precise(g_items, &rpt, heap, MAX_TOP_K);
    MatchResult results[MAX_TOP_K];
    //将前10条(目前是10，可以改TOP_K值取出更多或者更少)
    int k = heap_top_k(heap, MAX_TOP_K, results, MAX_TOP_K);

    printf("\n**精确寻物完成，候选 %d 条，展示前 %d 条（按得分排序）:\n", n, k);
    //打印候选结果，释放堆
    print_match_results(results, k);
    heap_destroy(heap);
}

//模糊寻物
static void menu_search_fuzzy()
{
    LostReport rpt;
    memset(&rpt, 0, sizeof(rpt));
    fill_lost_report_base(&rpt);

    char timebuf[64];
    read_line("--丢失时间起 (可回车跳过, YYYY-MM-DD HH:MM): ", timebuf, sizeof(timebuf));
    if (timebuf[0]) parse_datetime(timebuf, &rpt.lostTimeStart);

    read_line("--丢失时间止 (可回车跳过): ", timebuf, sizeof(timebuf));
    if (timebuf[0]) parse_datetime(timebuf, &rpt.lostTimeEnd);

    read_line("--丢失地点(可填「未知」或回车跳过): ", rpt.lostLocation, LOC_LEN);

    MaxHeap *heap = heap_create(MAX_TOP_K * 8);
    if (!heap) return;

    //模糊匹配逻辑
    int n = match_fuzzy(g_items, &rpt, heap, MAX_TOP_K);
    MatchResult results[MAX_TOP_K];
    int k = heap_top_k(heap, MAX_TOP_K, results, MAX_TOP_K);

    printf("\n--模糊寻物完成，候选 %d 条，展示前 %d 条:\n", n, k);
    print_match_results(results, k);
    heap_destroy(heap);
}

//提交认领
static void menu_claim_submit(void)
{
    char itemId[ID_LEN];
    char answer[SECRET_LEN];
    LostReport owner;
    memset(&owner, 0, sizeof(owner));

    read_line("--要领回的物品编号: ", itemId, ID_LEN);
    read_line("--失主姓名: ", owner.ownerName, NAME_LEN);
    read_line("--联系电话: ", owner.phone, PHONE_LEN);
    read_line("--独有特征答案: ", answer, SECRET_LEN);

    Item *it = store_find_by_id(g_items, itemId);
    if (it) {
        owner.itemName[0] = '\0';
        strncpy(owner.itemName, it->name, NAME_LEN - 1);
        strncpy(owner.category, it->category, CATEGORY_LEN - 1);
        strncpy(owner.lostLocation, it->location, LOC_LEN - 1);
    }

    claim_submit(g_claims, g_items, itemId, &owner, answer);
}

//管理员审核
static void menu_claim_review(void)
{
    char buf[16];
    read_line("--是否通过审核？(y/n): ", buf, sizeof(buf));
    int approve = (buf[0] == 'y' || buf[0] == 'Y');
    claim_review_front(g_claims, g_items, approve);
}

//按照编号查找
static void menu_query_by_id(void)
{
    char itemId[ID_LEN];
    read_line("--请输入物品编号: ", itemId, ID_LEN);
    Item *it = store_find_by_id(g_items, itemId);
    if (!it) {
        printf("--未找到该物品。\n");
        return;
    }
    char tbuf[32];
    struct tm *tm_info = localtime(&it->foundTime);
    //按照%Y-%m-%d %H:%M形式格式化日期
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M", tm_info);

    const char *st = "在库";
    if (it->status == ITEM_CLAIMED) st = "已认领";
    else if (it->status == ITEM_ARCHIVED) st = "已归档";

    printf("\n--编号: %s\n", it->itemId);
    printf("--名称: %s\n", it->name);
    printf("--类别: %s\n", it->category);
    printf("--交到招领处时间: %s\n", tbuf);
    printf("--地点: %s\n", it->location);
    printf("--描述: %s\n", it->description);
    printf("--状态: %s\n", st);
    printf("--（保密特征不在此显示）\n");
}

static void menu_browse(void)
{
    printf("\n  ===== 在库物品列表 =====\n");
    store_browse_in_storage(g_items);
}

int main(void)
{
    console_setup_utf8();

    //创建失物仓库和认领仓库
    g_items = store_create();
    g_claims = claim_store_create();
    if (!g_items || !g_claims) {
        fprintf(stderr, "初始化失败，内存不足。\n");
        return 1;
    }

    while(1) {
        print_menu();
        char choice[16];
        if (!fgets(choice, sizeof(choice), stdin)) break;
        trim_newline(choice);

        switch (choice[0]) {
            case '1': 
                menu_register(); 
                break;
            case '2': 
                menu_search_precise(); 
                break;
            case '3': 
                menu_search_fuzzy(); 
                break;
            case '4': 
                menu_claim_submit(); 
                break;
            case '5': 
                menu_claim_review(); 
                break;
            case '6': 
                menu_query_by_id(); 
                break;
            case '7': 
                menu_browse(); 
                break;
            case '0':
                printf("\n感谢使用，再见！\n\n");
                claim_store_destroy(g_claims);
                store_destroy(g_items);
                return 0;
            default:
                printf("...无效选项，请重新输入。\n");
                break;
        }
    }

    //销毁仓库
    claim_store_destroy(g_claims);
    store_destroy(g_items);
    return 0;
}
