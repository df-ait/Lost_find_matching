#ifndef QUEUE_H
#define QUEUE_H

#include "types.h"
//链队列
typedef struct QueueNode {
    ClaimRequest req;//物品状态
    struct QueueNode *next;
} QueueNode;

typedef struct {
    QueueNode *front;
    QueueNode *rear;
    int count;//队列中元素数量
} ClaimQueue;

ClaimQueue *queue_create(void);
void        queue_destroy(ClaimQueue *q);
//入队
int         queue_enqueue(ClaimQueue *q, const ClaimRequest *req);
//出队
int         queue_dequeue(ClaimQueue *q, ClaimRequest *out);
//得到队首
int         queue_peek(const ClaimQueue *q, ClaimRequest *out);
//判空
int         queue_empty(const ClaimQueue *q);

#endif /* QUEUE_H */
