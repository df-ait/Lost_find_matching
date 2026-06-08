//认领管理 — 申请、审核、全局队列
#ifndef CLAIM_MGR_H
#define CLAIM_MGR_H

#include "types.h"
#include "queue.h"
#include "item_mgr.h"

//认领申请用队列存储
typedef struct {
    ClaimQueue *pending;
    int         claim_counter;
} ClaimStore;

//创建&&销毁
ClaimStore *claim_store_create(void);
void claim_store_destroy(ClaimStore *cs);
//提交认领
int claim_submit(ClaimStore *cs, ItemStore *items,
                         const char *itemId, const LostReport *owner_info,
                         const char *answer_secret);
int claim_review_front(ClaimStore *cs, ItemStore *items, int approve);

#endif /* CLAIM_MGR_H */
