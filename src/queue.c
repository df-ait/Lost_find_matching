#include <stdlib.h>
#include "queue.h"

ClaimQueue *queue_create(void)
{
    ClaimQueue *q = (ClaimQueue *)malloc(sizeof(ClaimQueue));
    if (!q) return NULL;
    q->front = q->rear = NULL;
    //rear指向的是下一个进入的元素的位置
    q->count = 0;
    return q;
}

void queue_destroy(ClaimQueue *q)
{
    if (!q) return;
    while (!queue_empty(q)) {
        //全部出队以后再free队列本身
        ClaimRequest tmp;
        queue_dequeue(q, &tmp);
    }
    free(q);
}

int queue_enqueue(ClaimQueue *q, const ClaimRequest *req)
{
    if (!q || !req) return -1;
    QueueNode *node = (QueueNode *)malloc(sizeof(QueueNode));
    if (!node) return -1;
    node->req = *req;
    node->next = NULL;
    if (q->rear) {
        q->rear->next = node;
        q->rear = node;
    } else {
        //插入的第一个元素
        q->front = q->rear = node;
    }
    q->count++;
    return 0;
}

int queue_dequeue(ClaimQueue *q, ClaimRequest *out)
{
    if (!q || !out || !q->front) return -1;
    QueueNode *node = q->front;
    *out = node->req;
    q->front = node->next;
    //这个if针对的是队列中只有一个节点，出队以后就为空队列的情况
    if (!q->front) q->rear = NULL;
    free(node);
    q->count--;
    return 0;
}

int queue_peek(const ClaimQueue *q, ClaimRequest *out)
{
    if (!q || !out || !q->front) return -1;
    *out = q->front->req;
    return 0;
}

int queue_empty(const ClaimQueue *q)
{
    //队列为空 or 队列中元素为0 返回 TRUE 表示为空
    return !q || q->count == 0;
}
