//双向链表
#ifndef LIST_H
#define LIST_H

#include "types.h"

typedef struct ListNode {
    Item item;
    struct ListNode *prev;
    struct ListNode *next;
} ListNode;

typedef struct {
    ListNode *head;
    ListNode *tail;
    int count;
} ItemList;

//创建链表头
ItemList *list_create(void);
//销毁链表
void      list_destroy(ItemList *list);
//加入元素
int       list_append(ItemList *list, const Item *item);
//通过物品id找节点
ListNode *list_find_by_id(ItemList *list, const char *itemId);
//移除对应id的节点
int       list_remove_by_id(ItemList *list, const char *itemId);
//遍历链表，放了一个函数指针，当遍历每个物品的时候调用fn函数
void      list_foreach(const ItemList *list,
                       void (*fn)(const Item *item, void *ctx),
                       void *ctx);

#endif /* LIST_H */
