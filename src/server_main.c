#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "console_utf8.h"
#include "types.h"
#include "item_mgr.h"
#include "claim_mgr.h"
#include "match.h"
#include "ui_util.h"
#include "net_util.h"
#include "protocol.h"
#include "item_persist.h"

static ItemStore  *g_items  = NULL;
static ClaimStore *g_claims = NULL;
//线程锁
static pthread_mutex_t g_store_mutex = PTHREAD_MUTEX_INITIALIZER;

//服务端是否需要继续运行
static volatile int g_running = 1;
static int g_listen_fd = -1;

//加入新的失物
static void admin_menu_register(void)
{
    Item item;
    memset(&item, 0, sizeof(item));

    char timebuf[64];
    ui_read_line("--物品名称: ", item.name, NAME_LEN);
    ui_read_line("--物品类别(如钱包/钥匙/手机): ", item.category, CATEGORY_LEN);
    ui_read_line("--交到招领处时间 (YYYY-MM-DD HH:MM): ", timebuf, sizeof(timebuf));
    if (ui_parse_datetime(timebuf, &item.foundTime) != 0) {
        printf("!!时间格式错误，请使用 YYYY-MM-DD HH:MM（如 2026-05-18 14:30）。\n");
        return;
    }
    ui_read_line("--拾取/交到地点: ", item.location, LOC_LEN);
    ui_read_line("--外观描述(对外可见): ", item.description, DESC_LEN);
    ui_read_line("--独有特征(保密，可回车跳过；跳过则认领须管理员人工审核): ",
                 item.secretFeatures, SECRET_LEN);
    item_bind_secret(&item);

    item.status = ITEM_IN_STORAGE;
    item.registerTime = time(NULL);

    pthread_mutex_lock(&g_store_mutex);
    g_items->id_counter++;
    generate_item_id(item.itemId, g_items->id_counter);
    int rc = store_register_item(g_items, &item);
    if (rc == 0)
        store_save_items(g_items, ITEM_PERSIST_PATH);
    else
        g_items->id_counter--;
    pthread_mutex_unlock(&g_store_mutex);

    if (rc == 0) {
        printf("\n--登记成功！物品编号: %s\n", item.itemId);
        if (!item.has_secret)
            printf("--提示：未录入保密特征，失主认领时将跳过特征校验，须管理员人工审核。\n");
    } else {
        printf("\n--登记失败，请重试。\n");
    }
}

//审核：先展示队首认领单与物品信息，再由管理员决定
static void admin_menu_review(void)
{
    pthread_mutex_lock(&g_store_mutex);
    if (claim_show_pending_front(g_claims, g_items) != 0) {
        pthread_mutex_unlock(&g_store_mutex);
        return;
    }
    pthread_mutex_unlock(&g_store_mutex);

    char buf[16];
    ui_read_line("\n--是否通过审核？(y/n): ", buf, sizeof(buf));
    int approve = (buf[0] == 'y' || buf[0] == 'Y');

    pthread_mutex_lock(&g_store_mutex);
    claim_process_front(g_claims, g_items, approve);
    pthread_mutex_unlock(&g_store_mutex);
}

//按照编号查找物品
static void admin_menu_query(void)
{
    char itemId[ID_LEN];
    ui_read_line("--请输入物品编号: ", itemId, ID_LEN);

    //加锁查找，hash当中找
    pthread_mutex_lock(&g_store_mutex);
    Item *it = store_find_by_id(g_items, itemId);
    if (!it) {
        pthread_mutex_unlock(&g_store_mutex);
        printf("--未找到该物品。\n");
        return;
    }
    Item copy = *it;
    pthread_mutex_unlock(&g_store_mutex);

    char tbuf[32];
    struct tm *tm_info = localtime(&copy.foundTime);
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M", tm_info);

    const char *st = "在库";
    if (copy.status == ITEM_CLAIMED) st = "已认领";
    else if (copy.status == ITEM_ARCHIVED) st = "已归档";

    printf("\n--编号: %s\n", copy.itemId);
    printf("--名称: %s\n", copy.name);
    printf("--类别: %s\n", copy.category);
    printf("--交到招领处时间: %s\n", tbuf);
    printf("--地点: %s\n", copy.location);
    printf("--描述: %s\n", copy.description);
    printf("--状态: %s\n", st);
    if (copy.has_secret)
        printf("--保密特征: %s\n", copy.secretFeatures);
    else
        printf("--保密特征: （未录入，认领须人工审核）\n");
    if (copy.status == ITEM_CLAIMED) {
        char cbuf[32];
        if (copy.claimTime > 0) {
            struct tm *ct = localtime(&copy.claimTime);
            strftime(cbuf, sizeof(cbuf), "%Y-%m-%d %H:%M", ct);
            printf("--认领人: %s\n", copy.claimerName);
            printf("--认领电话: %s\n", copy.claimerPhone);
            printf("--认领时间: %s\n", cbuf);
        } else {
            printf("--认领人: %s\n", copy.claimerName[0] ? copy.claimerName : "（未记录）");
        }
    }
}

//打印仓库内失物
static void admin_menu_browse(void)
{
    printf("\n  ===== 在库物品列表 =====\n");
    pthread_mutex_lock(&g_store_mutex);
    store_browse_in_storage(g_items);
    pthread_mutex_unlock(&g_store_mutex);
}

//打操作菜单
static void print_admin_menu(void)
{
    printf("\n========== 服务端（管理员）==========\n");
    printf("1. 登记拾物\n");
    printf("2. 审核认领\n");
    printf("3. 按编号查询\n");
    printf("4. 浏览在库列表\n");
    printf("0. 退出服务\n");
    printf("====================================\n");
    printf("请选择: ");
}

//接收客户端连接线程
static void *client_accept_thread(void *arg)
{
    while (g_running) {
        int client_fd = net_accept(g_listen_fd);
        if (client_fd < 0)
            continue;

        protocol_serve_client(client_fd, g_items, g_claims, &g_store_mutex);
        net_close(client_fd);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    int port = NET_DEFAULT_PORT;
    if (argc >= 2)
        port = atoi(argv[1]);

    console_setup_utf8();

    if (net_init() != 0) {
        fprintf(stderr, "网络初始化失败。\n");
        return 1;
    }

    g_items = store_create();
    g_claims = claim_store_create();
    if (!g_items || !g_claims) {
        fprintf(stderr, "初始化失败，内存不足。\n");
        return 1;
    }

    if(store_load_items(g_items, ITEM_PERSIST_PATH) != 0)
        fprintf(stderr, "警告：加载数据文件失败，将从空库开始。\n");
    else
        printf("--已从 %s 加载在库物品（当前 id_counter=%d）\n",
               ITEM_PERSIST_PATH, g_items->id_counter);

    g_listen_fd = net_listen_tcp(port);
    if (g_listen_fd < 0) {
        fprintf(stderr, "无法在端口 %d 启动监听。\n", port);
        store_destroy(g_items);
        claim_store_destroy(g_claims);
        net_cleanup();
        return 1;
    }

    pthread_t tid;
    if (pthread_create(&tid, NULL, client_accept_thread, NULL) != 0) {
        fprintf(stderr, "无法启动客户端接入线程。\n");
        net_close(g_listen_fd);
        store_destroy(g_items);
        claim_store_destroy(g_claims);
        net_cleanup();
        return 1;
    }

    printf("失物招领服务端已启动，监听端口 %d\n", port);
    printf("请在本窗口进行管理员操作；用户请运行客户端连接本机。\n");

    while (g_running) {
        print_admin_menu();
        char choice[16];
        if (!fgets(choice, sizeof(choice), stdin))
            break;
        ui_trim_newline(choice);

        switch (choice[0]) {
            case '1': 
                admin_menu_register(); 
                break;
            case '2': 
                admin_menu_review(); 
                break;
            case '3': 
                admin_menu_query(); 
                break;
            case '4': 
                admin_menu_browse(); 
                break;
            case '0':
                g_running = 0;
                net_close(g_listen_fd);
                g_listen_fd = -1;
                pthread_join(tid, NULL);
                printf("\n服务端已退出。\n");
                claim_store_destroy(g_claims);
                store_destroy(g_items);
                net_cleanup();
                return 0;
            default:
                printf("无效选项。\n");
                break;
        }
    }

    g_running = 0;
    net_close(g_listen_fd);
    pthread_join(tid, NULL);
    claim_store_destroy(g_claims);
    store_destroy(g_items);
    net_cleanup();
    return 0;
}
