//认领管理 — 申请、审核、全局队列
#ifndef CLAIM_MGR_H
#define CLAIM_MGR_H

#include "types.h"
#include "queue.h"
#include "item_mgr.h"

#include "types.h"
#include "queue.h"
#include "item_mgr.h"

//认领申请用队列存储
typedef struct {
    ClaimQueue *pending;
    int         claim_counter;
} ClaimStore;

ClaimStore *claim_store_create(void);
void        claim_store_destroy(ClaimStore *cs);

int claim_submit(ClaimStore *cs, ItemStore *items,
                 const char *itemId, const LostReport *owner_info,
                 const char *answer_secret);
int claim_submit_msg(ClaimStore *cs, ItemStore *items,
                     const char *itemId, const LostReport *owner_info,
                     const char *answer_secret,
                     char *msg, size_t msg_len);
/* 展示队首认领单及关联物品信息，供管理员审阅；无待审单返回 -1 */
int claim_show_pending_front(ClaimStore *cs, ItemStore *items);
/* 对队首认领单执行通过/拒绝（调用前应先展示信息） */
int claim_process_front(ClaimStore *cs, ItemStore *items, int approve);
int claim_review_front(ClaimStore *cs, ItemStore *items, int approve);

#endif /* CLAIM_MGR_H */
