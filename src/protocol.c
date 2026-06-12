#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "protocol.h"
#include "net_util.h"
#include "match.h"
#include "heap.h"

#define PROTO_END "END"

//去除换行
static void proto_sanitize_field(char *s)
{
    if (!s) return;
    for (char *p = s; *p; p++)
        if (*p == '\t' || *p == '\n' || *p == '\r')
            *p = ' ';
}

//从TCP连接中读取多行文本，存储到字符串数组中，遇到结束标记时停止。
static int proto_read_block(int fd, char lines[][256], int max_lines)
{
    int n = 0;
    char buf[512];
    while (n < max_lines) {
        if (net_recv_line(fd, buf, sizeof(buf)) != 0)
            return -1;
        //看是否收到结束标记
        if (strcmp(buf, PROTO_END) == 0)
            return n;
        strncpy(lines[n], buf, 255);
        lines[n][255] = '\0';
        n++;
    }
    return -1;
}

//按照协议格式向socket连接发送一个标准的错误响应块，且返回-1表示错误状态
static int proto_send_err(int fd, const char *msg)
{
    net_send_line(fd, "ERR");
    net_send_line(fd, msg ? msg : "未知错误");
    net_send_line(fd, PROTO_END);
    return -1;
}
//发送正确响应块，返回0表示正确状态
static int proto_send_ok_line(int fd, const char *line)
{
    net_send_line(fd, "OK");
    net_send_line(fd, line ? line : "");
    net_send_line(fd, PROTO_END);
    return 0;
}

//将协议接收到的文本行数组转换为结构体数据
static void proto_fill_report(LostReport *rpt, char lines[][256], int n)
{
    //n为有效行数
    memset(rpt, 0, sizeof(*rpt));
    if(n > 0) strncpy(rpt->itemName, lines[0], NAME_LEN - 1);
    if(n > 1) strncpy(rpt->category, lines[1], CATEGORY_LEN - 1);
    if(n > 2) strncpy(rpt->ownerName, lines[2], NAME_LEN - 1);
    if(n > 3) strncpy(rpt->phone, lines[3], PHONE_LEN - 1);
    if(n > 4) strncpy(rpt->description, lines[4], DESC_LEN - 1);
    if(n > 5 && lines[5][0]){
        time_t t;
        if(sscanf(lines[5], "%lld", (long long *)&t) == 1){
            rpt->lostTimeStart = t;
        }
    }
    if(n > 6 && lines[6][0]){
        time_t t;
        if(sscanf(lines[6], "%lld", (long long *)&t) == 1){
            rpt->lostTimeEnd = t;
        }
    }
    if(n > 7) strncpy(rpt->lostLocation, lines[7], LOC_LEN - 1);
}

//发送回检查结果
static int proto_send_search_results(int fd, int total, int k,
                                     const MatchResult *results){
    char buf[64];
    //成功标识
    net_send_line(fd, "OK");
    snprintf(buf, sizeof(buf), "%d", total);
    //总匹配数，取大根堆的前K条数据了
    net_send_line(fd, buf);
    snprintf(buf, sizeof(buf), "%d", k);
    //本次返回数
    net_send_line(fd, buf);

    for (int i = 0; i < k; i++) {
        const MatchResult *r = &results[i];
        const char *st = "在库";
        if (r->item.status == ITEM_CLAIMED) st = "已认领";
        else if (r->item.status == ITEM_ARCHIVED) st = "已归档";

        char line[1024];
        char name[NAME_LEN], category[CATEGORY_LEN], loc[LOC_LEN], desc[DESC_LEN];
        strncpy(name, r->item.name, NAME_LEN - 1);
        strncpy(category, r->item.category, CATEGORY_LEN - 1);
        strncpy(loc, r->item.location, LOC_LEN - 1);
        strncpy(desc, r->item.description, DESC_LEN - 1);
        name[NAME_LEN - 1] = '\0'; 
        category[CATEGORY_LEN - 1] = '\0';
        loc[LOC_LEN - 1] = '\0';
        desc[DESC_LEN - 1] = '\0';
        proto_sanitize_field(name);
        proto_sanitize_field(category);
        proto_sanitize_field(loc);
        proto_sanitize_field(desc);

        snprintf(line, sizeof(line), "%s\t%.1f\t%s\t%s\t%s\t%s\t%s",
                 r->itemId, r->score, name, category, st, loc, desc);
        net_send_line(fd, line);
    }
    //所有K条记录都发送完了需要发一个结束符，好让客户端知道什么时候停止读取
    net_send_line(fd, PROTO_END);
    return 0;
}

//接收申请，转换为结构体，精确查找，返回结果
static int proto_handle_search_precise(int fd, ItemStore *items,
                                       char lines[][256], int n){
    LostReport rpt;
    proto_fill_report(&rpt, lines, n);

    if(n > 5 && lines[5][0] && rpt.lostTimeStart == 0){
        time_t t;
        struct tm tmv;
        memset(&tmv, 0, sizeof(tmv));
        int y, mo, d, h, mi;
        if(sscanf(lines[5], "%d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) == 5){
            tmv.tm_year = y - 1900;
            //tm按0~11存月，但是用户输入的是1~12，所以要 mo - 1
            tmv.tm_mon = mo - 1;
            tmv.tm_mday = d;
            tmv.tm_hour = h;
            tmv.tm_min = mi;
            tmv.tm_isdst = -1;
            t = mktime(&tmv);
            rpt.lostTimeStart = t - TIME_WINDOW_SEC;
            rpt.lostTimeEnd = t + TIME_WINDOW_SEC;
        }
    }

    MaxHeap *heap = heap_create(MAX_TOP_K * 8);
    if (!heap) return proto_send_err(fd, "服务器内存不足");

    //匹配查找物品，精确寻物
    int total = match_precise(items, &rpt, heap, MAX_TOP_K);
    MatchResult results[MAX_TOP_K];
    int k = heap_top_k(heap, MAX_TOP_K, results, MAX_TOP_K);
    heap_destroy(heap);

    return proto_send_search_results(fd, total, k, results);
}

////接收申请，转换为结构体，模糊查找，返回结果
static int proto_handle_search_fuzzy(int fd, ItemStore *items,
                                     char lines[][256], int n){
    LostReport rpt;
    proto_fill_report(&rpt, lines, n);

    //看n的行数判断有没有填写 起始时间，终止时间
    if (n > 5 && lines[5][0] && rpt.lostTimeStart == 0) {
        time_t t;
        struct tm tmv;
        memset(&tmv, 0, sizeof(tmv));
        int y, mo, d, h, mi;
        if (sscanf(lines[5], "%d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) == 5) {
            tmv.tm_year = y - 1900;
            tmv.tm_mon = mo - 1;
            tmv.tm_mday = d;
            tmv.tm_hour = h;
            tmv.tm_min = mi;
            tmv.tm_isdst = -1;
            t = mktime(&tmv);
            rpt.lostTimeStart = t;
        }
    }
    if (n > 6 && lines[6][0] && rpt.lostTimeEnd == 0) {
        time_t t;
        struct tm tmv;
        memset(&tmv, 0, sizeof(tmv));
        int y, mo, d, h, mi;
        if (sscanf(lines[6], "%d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) == 5) {
            tmv.tm_year = y - 1900;
            tmv.tm_mon = mo - 1;
            tmv.tm_mday = d;
            tmv.tm_hour = h;
            tmv.tm_min = mi;
            tmv.tm_isdst = -1;
            t = mktime(&tmv);
            rpt.lostTimeEnd = t;
        }
    }

    MaxHeap *heap = heap_create(MAX_TOP_K * 8);
    if (!heap) return proto_send_err(fd, "服务器内存不足");

    int total = match_fuzzy(items, &rpt, heap, MAX_TOP_K);
    MatchResult results[MAX_TOP_K];
    int k = heap_top_k(heap, MAX_TOP_K, results, MAX_TOP_K);
    heap_destroy(heap);

    return proto_send_search_results(fd, total, k, results);
}

//(#`O′)********************
//接收客户端传来的物品ID，在数据库中查找对应物品，如果找到则返回详细信息，否则返回错误
static int proto_handle_lookup(int fd, ItemStore *items,
                               char lines[][256], int n){
    if(n < 1 || !lines[0][0])
        return proto_send_err(fd, "缺少物品编号");

    Item *it = store_find_by_id(items, lines[0]);
    if(!it)
        return proto_send_err(fd, "未找到该物品");

    char line[1024];
    char name[NAME_LEN], category[CATEGORY_LEN], loc[LOC_LEN], desc[DESC_LEN];
    strncpy(name, it->name, NAME_LEN - 1);
    strncpy(category, it->category, CATEGORY_LEN - 1);
    strncpy(loc, it->location, LOC_LEN - 1);
    strncpy(desc, it->description, DESC_LEN - 1);
    proto_sanitize_field(name);
    proto_sanitize_field(category);
    proto_sanitize_field(loc);
    proto_sanitize_field(desc);

    const char *st = "在库";
    if (it->status == ITEM_CLAIMED) st = "已认领";
    else if (it->status == ITEM_ARCHIVED) st = "已归档";

    snprintf(line, sizeof(line), "%s\t%s\t%s\t%s\t%s\t%d",
             it->itemId, name, category, st, loc,
             it->has_secret);
    (void)desc;
    return proto_send_ok_line(fd, line);
}

//认领
static int proto_handle_claim(int fd, ItemStore *items, ClaimStore *claims,
                              char lines[][256], int n){
    if (n < 3)
        return proto_send_err(fd, "认领参数不足");

    char itemId[ID_LEN];
    LostReport owner;
    char answer[SECRET_LEN];
    memset(&owner, 0, sizeof(owner));

    strncpy(itemId, lines[0], ID_LEN - 1);
    strncpy(owner.ownerName, lines[1], NAME_LEN - 1);
    strncpy(owner.phone, lines[2], PHONE_LEN - 1);
    answer[0] = '\0';
    if (n > 3) strncpy(answer, lines[3], SECRET_LEN - 1);

    Item *it = store_find_by_id(items, itemId);
    if (it) {
        strncpy(owner.itemName, it->name, NAME_LEN - 1);
        strncpy(owner.category, it->category, CATEGORY_LEN - 1);
        strncpy(owner.lostLocation, it->location, LOC_LEN - 1);
    }

    char msg[512];
    int rc = claim_submit_msg(claims, items, itemId, &owner, answer,
                              msg, sizeof(msg));
    if (rc != 0)
        return proto_send_err(fd, msg[0] ? msg : "认领失败");
    return proto_send_ok_line(fd, msg);
}

//总流程
void protocol_serve_client(int fd, ItemStore *items, ClaimStore *claims){
    char cmd[64];
    char lines[16][256];

    if(net_recv_line(fd, cmd, sizeof(cmd)) != 0)
        return;

    int n = proto_read_block(fd, lines, 16);
    if(n < 0){
        proto_send_err(fd, "请求格式错误");
        return;
    }

    if(strcmp(cmd, "SEARCH_PRECISE") == 0)
        proto_handle_search_precise(fd, items, lines, n);
    else if(strcmp(cmd, "SEARCH_FUZZY") == 0)
        proto_handle_search_fuzzy(fd, items, lines, n);
    else if(strcmp(cmd, "LOOKUP") == 0)
        proto_handle_lookup(fd, items, lines, n);
    else if(strcmp(cmd, "CLAIM") == 0)
        proto_handle_claim(fd, items, claims, lines, n);
    else if(strcmp(cmd, "PING") == 0)
        proto_send_ok_line(fd, "PONG");
    else
        proto_send_err(fd, "未知命令");
}

//------------------------------客户端---------------------------------------------
//收响应第一行，判断成功还是失败
static int proto_expect_ok(int fd, char *err, size_t err_len){
    char line[512];
    if(net_recv_line(fd, line, sizeof(line)) != 0){
        if (err && err_len) snprintf(err, err_len, "连接已断开");
        return -1;
    }
    if(strcmp(line, "OK") == 0)
        return 0;
    if(strcmp(line, "ERR") == 0){
        if (net_recv_line(fd, line, sizeof(line)) == 0 && err && err_len)
            snprintf(err, err_len, "%s", line);
        net_recv_line(fd, line, sizeof(line));
        return -1;
    }
    if(err && err_len) snprintf(err, err_len, "协议响应异常");
    return -1;
}

//按协议格式,发一整包请求
static int proto_send_request(int fd, const char *cmd,
                              const char *fields[], int field_count){
    if(net_send_line(fd, cmd) != 0) return -1;
    for(int i = 0; i < field_count; i++){
        if(net_send_line(fd, fields[i] ? fields[i] : "") != 0)
            return -1;
    }
    return net_send_line(fd, PROTO_END);
}

////////
static int client_recv_search(int fd, char *err, size_t err_len){
    char line[512];
    if(net_recv_line(fd, line, sizeof(line)) != 0) goto fail;

    int total = 0, k = 0;
    if(strcmp(line, "OK") != 0) {
        if(strcmp(line, "ERR") == 0) {
            net_recv_line(fd, line, sizeof(line));
            if(err && err_len) snprintf(err, err_len, "%s", line);
            net_recv_line(fd, line, sizeof(line));
        }
        return -1;
    }

    if(net_recv_line(fd, line, sizeof(line)) != 0) goto fail;
    total = atoi(line);
    if(net_recv_line(fd, line, sizeof(line)) != 0) goto fail;
    k = atoi(line);

    printf("\n--寻物完成，候选 %d 条，展示前 %d 条（按得分排序）:\n", total, k);
    if (k <= 0) {
        printf("!!（无匹配结果）\n");
        net_recv_line(fd, line, sizeof(line));
        return 0;
    }

    printf("\n  %-14s %-8s %-12s %-18s %-10s %s\n",
           "编号", "得分", "名称", "类别", "状态", "地点");
    printf("  %s\n", "----------------------------------------------------------------");

    for (int i = 0; i < k; i++) {
        if (net_recv_line(fd, line, sizeof(line)) != 0) goto fail;
        char itemId[ID_LEN], name[NAME_LEN], category[CATEGORY_LEN];
        char st[32], loc[LOC_LEN], desc[DESC_LEN];
        double score;
        if (sscanf(line, "%31[^\t]\t%lf\t%63[^\t]\t%31[^\t]\t%31[^\t]\t%63[^\t]\t%255[^\t]",
                   itemId, &score, name, category, st, loc, desc) >= 6) {
            printf("  %-14s %6.1f   %-12s %-18s %-10s %s\n",
                   itemId, score, name, category, st, loc);
            if (desc[0]) printf("    描述: %s\n", desc);
        }
    }
    net_recv_line(fd, line, sizeof(line));
    return 0;

fail:
    if (err && err_len) snprintf(err, err_len, "接收结果失败");
    return -1;
}

int client_search_precise(int fd, const LostReport *rpt, char *err, size_t err_len)
{
    char tbuf[64] = "";
    char tstart[32] = "", tend[32] = "";
    if (rpt->lostTimeStart && rpt->lostTimeEnd) {
        snprintf(tstart, sizeof(tstart), "%lld", (long long)rpt->lostTimeStart);
        snprintf(tend, sizeof(tend), "%lld", (long long)rpt->lostTimeEnd);
    }

    const char *fields[] = {
        rpt->itemName, rpt->category, rpt->ownerName, rpt->phone,
        rpt->description, tstart[0] ? tstart : tbuf, tend, rpt->lostLocation
    };
    if (proto_send_request(fd, "SEARCH_PRECISE", fields, 8) != 0) {
        if (err && err_len) snprintf(err, err_len, "发送请求失败");
        return -1;
    }
    return client_recv_search(fd, err, err_len);
}

int client_search_fuzzy(int fd, const LostReport *rpt, char *err, size_t err_len)
{
    char tstart[32] = "", tend[32] = "";
    if (rpt->lostTimeStart)
        snprintf(tstart, sizeof(tstart), "%lld", (long long)rpt->lostTimeStart);
    if (rpt->lostTimeEnd)
        snprintf(tend, sizeof(tend), "%lld", (long long)rpt->lostTimeEnd);

    const char *fields[] = {
        rpt->itemName, rpt->category, rpt->ownerName, rpt->phone,
        rpt->description, tstart, tend, rpt->lostLocation
    };
    if (proto_send_request(fd, "SEARCH_FUZZY", fields, 8) != 0) {
        if (err && err_len) snprintf(err, err_len, "发送请求失败");
        return -1;
    }
    return client_recv_search(fd, err, err_len);
}

int client_lookup_item(int fd, const char *itemId, Item *out_public,
                       int *has_secret, char *err, size_t err_len)
{
    const char *fields[] = { itemId };
    if (proto_send_request(fd, "LOOKUP", fields, 1) != 0) {
        if (err && err_len) snprintf(err, err_len, "发送请求失败");
        return -1;
    }

    char line[512];
    if (proto_expect_ok(fd, err, err_len) != 0)
        return -1;
    if (net_recv_line(fd, line, sizeof(line)) != 0)
        return -1;

    if (out_public) {
        memset(out_public, 0, sizeof(*out_public));
        char st[32];
        int hs = 0;
        sscanf(line, "%31[^\t]\t%63[^\t]\t%31[^\t]\t%31[^\t]\t%63[^\t]\t%d",
               out_public->itemId, out_public->name, out_public->category,
               st, out_public->location, &hs);
        if (strcmp(st, "已认领") == 0) out_public->status = ITEM_CLAIMED;
        else if (strcmp(st, "已归档") == 0) out_public->status = ITEM_ARCHIVED;
        else out_public->status = ITEM_IN_STORAGE;
        if (has_secret) *has_secret = hs;
    }
    net_recv_line(fd, line, sizeof(line));
    return 0;
}

int client_claim_submit(int fd, const char *itemId, const LostReport *owner,
                        const char *answer, char *msg, size_t msg_len)
{
    const char *fields[] = { itemId, owner->ownerName, owner->phone, answer };
    if (proto_send_request(fd, "CLAIM", fields, 4) != 0) {
        if (msg && msg_len) snprintf(msg, msg_len, "发送请求失败");
        return -1;
    }

    char line[512];
    if (net_recv_line(fd, line, sizeof(line)) != 0) {
        if (msg && msg_len) snprintf(msg, msg_len, "接收响应失败");
        return -1;
    }
    if (strcmp(line, "OK") == 0) {
        if (net_recv_line(fd, line, sizeof(line)) == 0 && msg && msg_len)
            snprintf(msg, msg_len, "%s", line);
        net_recv_line(fd, line, sizeof(line));
        return 0;
    }
    if (strcmp(line, "ERR") == 0) {
        net_recv_line(fd, line, sizeof(line));
        if (msg && msg_len) snprintf(msg, msg_len, "%s", line);
        net_recv_line(fd, line, sizeof(line));
        return -1;
    }
    if (msg && msg_len) snprintf(msg, msg_len, "协议响应异常");
    return -1;
}
