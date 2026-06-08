//认领申请与审核
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "claim_mgr.h"
#include "match.h"

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
    if (!cs || !items || !itemId || !owner_info || !answer_secret)
        return -1;

    Item *item = store_find_by_id(items, itemId);
    if (!item) {
        printf("  错误：物品编号不存在。\n");
        return -1;
    }
    if (item->status != ITEM_IN_STORAGE) {
        printf("  错误：该物品不在库，无法认领。\n");
        return -1;
    }

    double score = calc_match_score(item, owner_info);
    int secret_ok = verify_secret(item, answer_secret);

    if (score < MATCH_THRESHOLD) {
        printf("  认领失败：匹配得分 %.1f 低于阈值 %.1f。\n", score, MATCH_THRESHOLD);
        return -1;
    }
    if (!secret_ok) {
        printf("  认领失败：独有特征验证未通过，请补充更准确的信息。\n");
        return -1;
    }

    ClaimRequest req;
    memset(&req, 0, sizeof(req));
    cs->claim_counter++;
    //生成寻物ID
    generate_claim_id(req.claimId, cs->claim_counter);
    //将这些信息复制到认领申请当中
    strncpy(req.itemId, itemId, ID_LEN - 1);
    strncpy(req.ownerName, owner_info->ownerName, NAME_LEN - 1);
    strncpy(req.phone, owner_info->phone, PHONE_LEN - 1);
    strncpy(req.answerSecret, answer_secret, SECRET_LEN - 1);
    req.matchScore = score;
    req.status = CLAIM_PENDING;

    if (queue_enqueue(cs->pending, &req) != 0)
        return -1;

    printf("  认领申请已提交！单号: %s，匹配分: %.1f，等待管理员审核。\n",
           req.claimId, score);
    return 0;
}

//审核队首一条
int claim_review_front(ClaimStore *cs, ItemStore *items, int approve)
{
    if (!cs || !items) return -1;
    ClaimRequest req;
    if (queue_peek(cs->pending, &req) != 0) {
        printf("--当前没有待审核的认领申请。\n");
        return -1;
    }

    Item *item = store_find_by_id(items, req.itemId);
    printf("\n===== 待审核认领 =====\n");
    printf("--认领单号: %s\n", req.claimId);
    printf("--物品编号: %s\n", req.itemId);
    if (item) {
        printf("--物品名称: %s\n", item->name);
        printf("--保密特征: %s\n", item->secretFeatures);
    }
    printf("--申请人: %s  --\n电话: %s\n", req.ownerName, req.phone);
    printf("--特征答案: %s\n", req.answerSecret);
    printf("--匹配得分: %.1f\n", req.matchScore);

    if (!approve) {
        queue_dequeue(cs->pending, &req);
        req.status = CLAIM_REJECTED;
        printf("  审核结果: 已拒绝\n");
        return 0;
    }

    queue_dequeue(cs->pending, &req);
    req.status = CLAIM_APPROVED;
    store_set_status(items, req.itemId, ITEM_CLAIMED);
    printf("  审核结果: 已通过，物品状态已更新为「已认领」\n");
    return 0;
}
