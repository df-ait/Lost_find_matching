//大根堆 — 匹配结果 Top-K 排序输出
#ifndef HEAP_H
#define HEAP_H

#include "types.h"

typedef struct {
    MatchResult *data;
    int size;//当前已存放的匹配结果
    int capacity;//最大容量
} MaxHeap;

MaxHeap *heap_create(int capacity);
void heap_destroy(MaxHeap *heap);
//移除某个节点
void heap_clear(MaxHeap *heap);
//添加某个节点
int heap_push(MaxHeap *heap, const MatchResult *result);
//获取堆中元素数量
int heap_size(const MaxHeap *heap);
//取出K个堆顶元素
int heap_top_k(MaxHeap *heap, int k, MatchResult *out, int out_cap);

#endif /* HEAP_H */
