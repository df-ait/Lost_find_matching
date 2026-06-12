#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "console_utf8.h"
#include "types.h"
#include "match.h"
#include "ui_util.h"
#include "net_util.h"
#include "protocol.h"

static int g_sock = -1;
static char g_host[64] = "127.0.0.1";
static int g_port = NET_DEFAULT_PORT;

/* 服务端每处理完一条请求就关闭连接，客户端每次发请求前须重新连接 */
static int connect_server(const char *host, int port)
{
    if (g_sock >= 0) {
        net_close(g_sock);
        g_sock = -1;
    }
    g_sock = net_connect_tcp(host, port);
    if (g_sock < 0) {
        printf("--无法连接服务器 %s:%d，请先启动服务端。\n",
               host && host[0] ? host : "127.0.0.1", port);
        return -1;
    }
    return 0;
}

static int ensure_connected(void)
{
    return connect_server(g_host, g_port);
}

static void fill_lost_report_base(LostReport *rpt)
{
    memset(rpt, 0, sizeof(*rpt));
    ui_read_line("--物品名称(可回车跳过): ", rpt->itemName, NAME_LEN);
    ui_read_line("--物品类别(可回车跳过): ", rpt->category, CATEGORY_LEN);
    ui_read_line("--失主姓名: ", rpt->ownerName, NAME_LEN);
    ui_read_line("--联系电话: ", rpt->phone, PHONE_LEN);
    ui_read_line("--补充描述(可回车跳过): ", rpt->description, DESC_LEN);
}

static void menu_search_precise(void)
{
    LostReport rpt;
    fill_lost_report_base(&rpt);

    char timebuf[64];
    ui_read_line("--丢失时间 (YYYY-MM-DD HH:MM): ", timebuf, sizeof(timebuf));
    time_t t;
    if (ui_parse_datetime(timebuf, &t) != 0) {
        printf("!!时间格式错误。\n");
        return;
    }
    rpt.lostTimeStart = t - TIME_WINDOW_SEC;
    rpt.lostTimeEnd   = t + TIME_WINDOW_SEC;
    ui_read_line("--丢失地点: ", rpt.lostLocation, LOC_LEN);

    if (ensure_connected() != 0)
        return;

    char err[256];
    if (client_search_precise(g_sock, &rpt, err, sizeof(err)) != 0)
        printf("--寻物失败: %s\n", err[0] ? err : "未知错误");
}

static void menu_search_fuzzy(void)
{
    LostReport rpt;
    fill_lost_report_base(&rpt);

    char timebuf[64];
    ui_read_line("--丢失时间起 (可回车跳过, YYYY-MM-DD HH:MM): ", timebuf, sizeof(timebuf));
    if (timebuf[0]) ui_parse_datetime(timebuf, &rpt.lostTimeStart);

    ui_read_line("--丢失时间止 (可回车跳过): ", timebuf, sizeof(timebuf));
    if (timebuf[0]) ui_parse_datetime(timebuf, &rpt.lostTimeEnd);

    ui_read_line("--丢失地点(可填「未知」或回车跳过): ", rpt.lostLocation, LOC_LEN);

    if (ensure_connected() != 0)
        return;

    char err[256];
    if (client_search_fuzzy(g_sock, &rpt, err, sizeof(err)) != 0)
        printf("--寻物失败: %s\n", err[0] ? err : "未知错误");
}

static void menu_claim_submit(void)
{
    char itemId[ID_LEN];
    char answer[SECRET_LEN];
    LostReport owner;
    memset(&owner, 0, sizeof(owner));
    answer[0] = '\0';

    ui_read_line("--要领回的物品编号: ", itemId, ID_LEN);
    ui_read_line("--失主姓名: ", owner.ownerName, NAME_LEN);
    ui_read_line("--联系电话: ", owner.phone, PHONE_LEN);

    Item pub;
    int has_secret = 1;
    char err[256];

    if (ensure_connected() != 0)
        return;
    if (client_lookup_item(g_sock, itemId, &pub, &has_secret, err, sizeof(err)) != 0) {
        printf("--查询物品失败: %s\n", err);
        return;
    }

    strncpy(owner.itemName, pub.name, NAME_LEN - 1);
    strncpy(owner.category, pub.category, CATEGORY_LEN - 1);
    strncpy(owner.lostLocation, pub.location, LOC_LEN - 1);

    if (!has_secret) {
        printf("--该物品登记时未录入保密特征，无需填写特征答案，提交后须管理员人工审核。\n");
    } else {
        ui_read_line("--独有特征答案: ", answer, SECRET_LEN);
    }

    /* LOOKUP 完成后服务端已断开，CLAIM 须用新连接 */
    if (ensure_connected() != 0)
        return;

    char msg[512];
    if (client_claim_submit(g_sock, itemId, &owner, answer, msg, sizeof(msg)) == 0)
        printf("--%s\n", msg);
    else
        printf("--认领失败: %s\n", msg[0] ? msg : "未知错误");
}

static void print_client_menu(void)
{
    printf("\n========== 客户端（公众）==========\n");
    printf("1. 精确寻物（时间+地点）\n");
    printf("2. 模糊寻物（名称/类别为主）\n");
    printf("3. 提交认领申请\n");
    printf("0. 退出\n");
    printf("==================================\n");
    printf("请选择: ");
}

int main(int argc, char **argv)
{
    if (argc >= 2) strncpy(g_host, argv[1], sizeof(g_host) - 1);
    if (argc >= 3) g_port = atoi(argv[2]);

    console_setup_utf8();

    if (net_init() != 0) {
        fprintf(stderr, "网络初始化失败。\n");
        return 1;
    }

    printf("失物招领客户端\n");
    if (ensure_connected() != 0) {
        net_cleanup();
        return 1;
    }
    printf("--已连接服务器 %s:%d（每次操作将自动重连）\n", g_host, g_port);

    while (1) {
        print_client_menu();
        char choice[16];
        if (!fgets(choice, sizeof(choice), stdin))
            break;
        ui_trim_newline(choice);

        switch (choice[0]) {
        case '1': menu_search_precise(); break;
        case '2': menu_search_fuzzy(); break;
        case '3': menu_claim_submit(); break;
        case '0':
            printf("\n再见！\n");
            net_close(g_sock);
            net_cleanup();
            return 0;
        default:
            printf("无效选项。\n");
            break;
        }
    }

    net_close(g_sock);
    net_cleanup();
    return 0;
}
