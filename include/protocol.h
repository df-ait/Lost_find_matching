#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <pthread.h>
#include "item_mgr.h"
#include "claim_mgr.h"

/* 先读网络请求，仅在访问共享数据时加锁 */
void protocol_serve_client(int fd, ItemStore *items, ClaimStore *claims,
                           pthread_mutex_t *store_mutex);

//客户端API--返回 0 成功，-1 失败；失败时 err 可选填原因
int client_search_precise(int fd, const LostReport *rpt, char *err, size_t err_len);
int client_search_fuzzy(int fd, const LostReport *rpt, char *err, size_t err_len);
int client_lookup_item(int fd, const char *itemId, Item *out_public,
                       int *has_secret, char *err, size_t err_len);
int client_claim_submit(int fd, const char *itemId, const LostReport *owner,
                        const char *answer, char *msg, size_t msg_len);

#endif /* PROTOCOL_H */
