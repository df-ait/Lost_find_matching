#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "claim_mgr.h"
#include "match.h"

static int claim_submit_core(ClaimStore *cs, ItemStore *items,
                               const char *itemId, const LostReport *owner_info,
                               const char *answer_secret,
                               char *msg, size_t msg_len)
{
    if (!cs || !items || !itemId || !owner_info || !answer_secret)
        return -1;

    Item *item = store_find_by_id(items, itemId);
    if (!item) {
        if (msg && msg_len)
            snprintf(msg, msg_len, "错误：物品编号不存在。");
        return -1;
    }
    if (item->status != ITEM_IN_STORAGE) {
        if (msg && msg_len)
            snprintf(msg, msg_len, "错误：该物品不在库，无法认领。");
        return -1;
    }

    double score = calc_match_score(item, owner_info);
    int has_secret = item->has_secret;

    if (score < MATCH_THRESHOLD) {
        if (msg && msg_len)
            snprintf(msg, msg_len, "认领失败：匹配得分 %.1f 低于阈值 %.1f。",
                     score, MATCH_THRESHOLD);
        return -1;
    }

    if (has_secret) {
        if (!verify_secret(item, answer_secret)) {
            if (msg && msg_len)
                snprintf(msg, msg_len, "认领失败：独有特征验证未通过，请补充更准确的信息。");
            return -1;
        }
    }

    ClaimRequest req;
    memset(&req, 0, sizeof(req));
    cs->claim_counter++;
    generate_claim_id(req.claimId, cs->claim_counter);
    strncpy(req.itemId, itemId, ID_LEN - 1);
    strncpy(req.ownerName, owner_info->ownerName, NAME_LEN - 1);
    strncpy(req.phone, owner_info->phone, PHONE_LEN - 1);
    strncpy(req.answerSecret, answer_secret, SECRET_LEN - 1);
    req.matchScore = score;
    req.status = CLAIM_PENDING;
    req.adminReviewOnly = has_secret ? 0 : 1;

    if (queue_enqueue(cs->pending, &req) != 0)
        return -1;

    if (msg && msg_len) {
        if (req.adminReviewOnly)
            snprintf(msg, msg_len,
                     "认领申请已提交！单号: %s，匹配分: %.1f。"
                     "该物品登记时未录入保密特征，须管理员人工审核。",
                     req.claimId, score);
        else
            snprintf(msg, msg_len,
                     "认领申请已提交！单号: %s，匹配分: %.1f，等待管理员审核。",
                     req.claimId, score);
    }
    return 0;
}

ClaimStore *claim_store_create(void)
{
    ClaimStore *cs = (ClaimStore *)malloc(sizeof(ClaimStore));
    if (!cs) return NULL;
    cs->pending = queue_create();
    cs->claim_counter = 0;
    if (!cs->pending) {
        free(cs);
        return NULL;
    }
    return cs;
}

void claim_store_destroy(ClaimStore *cs)
{
    if (!cs) return;
    queue_destroy(cs->pending);
    free(cs);
}

//成功提交认领返回0，否则返回-1
int claim_submit(ClaimStore *cs, ItemStore *items,
                 const char *itemId, const LostReport *owner_info,
                 const char *answer_secret)
{
    char msg[512];
    msg[0] = '\0';
    int rc = claim_submit_core(cs, items, itemId, owner_info, answer_secret,
                               msg, sizeof(msg));
    if (msg[0])
        printf("--%s\n", msg);
    return rc;
}

int claim_submit_msg(ClaimStore *cs, ItemStore *items,
                     const char *itemId, const LostReport *owner_info,
                     const char *answer_secret,
                     char *msg, size_t msg_len)
{
    return claim_submit_core(cs, items, itemId, owner_info, answer_secret,
                             msg, msg_len);
}

static void print_pending_review(const ClaimRequest *req, const Item *item)
{
    printf("\n===== 待审核认领 =====\n");
    printf("--认领单号: %s\n", req->claimId);
    printf("--匹配得分: %.1f\n", req->matchScore);
    if (req->adminReviewOnly)
        printf("--审核方式: **须管理员人工审核**（登记时无保密特征，无法自动验特征）\n");
    else
        printf("--审核方式: 已通过系统特征校验，请管理员复核\n");

    printf("\n----- 申请人信息 -----\n");
    printf("--姓名: %s\n", req->ownerName);
    printf("--电话: %s\n", req->phone);
    if (req->answerSecret[0])
        printf("--特征答案: %s\n", req->answerSecret);
    else
        printf("--特征答案: （未填写）\n");

    printf("\n----- 物品信息 -----\n");
    printf("--物品编号: %s\n", req->itemId);
    if (!item) {
        printf("--（物品记录不存在或已删除）\n");
        return;
    }

    char tbuf[32];
    struct tm *tm_info = localtime(&item->foundTime);
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M", tm_info);

    const char *st = "在库";
    if (item->status == ITEM_CLAIMED) st = "已认领";
    else if (item->status == ITEM_ARCHIVED) st = "已归档";

    printf("--名称: %s\n", item->name);
    printf("--类别: %s\n", item->category);
    printf("--交到招领处时间: %s\n", tbuf);
    printf("--地点: %s\n", item->location);
    printf("--描述: %s\n", item->description);
    printf("--状态: %s\n", st);
    if (item->has_secret)
        printf("--保密特征(登记): %s\n", item->secretFeatures);
    else
        printf("--保密特征(登记): （未录入）\n");
}

int claim_show_pending_front(ClaimStore *cs, ItemStore *items)
{
    if (!cs || !items) return -1;

    ClaimRequest req;
    if (queue_peek(cs->pending, &req) != 0) {
        printf("--当前没有待审核的认领申请。\n");
        return -1;
    }

    Item *item = store_find_by_id(items, req.itemId);
    print_pending_review(&req, item);
    return 0;
}

int claim_process_front(ClaimStore *cs, ItemStore *items, int approve)
{
    if (!cs || !items) return -1;

    ClaimRequest req;
    if (queue_peek(cs->pending, &req) != 0) {
        printf("--当前没有待审核的认领申请。\n");
        return -1;
    }

    if (!approve) {
        queue_dequeue(cs->pending, &req);
        req.status = CLAIM_REJECTED;
        printf("\n--审核结果: 已拒绝\n");
        return 0;
    }

    queue_dequeue(cs->pending, &req);
    req.status = CLAIM_APPROVED;
    store_set_status(items, req.itemId, ITEM_CLAIMED);
    printf("\n--审核结果: 已通过，物品状态已更新为「已认领」\n");
    return 0;
}

int claim_review_front(ClaimStore *cs, ItemStore *items, int approve)
{
    if (claim_show_pending_front(cs, items) != 0)
        return -1;
    return claim_process_front(cs, items, approve);
}
