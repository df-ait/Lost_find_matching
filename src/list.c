#include <stdlib.h>
#include <string.h>
#include "list.h"

ItemList *list_create()
{
    ItemList *list = (ItemList *)malloc(sizeof(ItemList));
    if (!list) return NULL;
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
    return list;
}

void list_destroy(ItemList *list)
{
    if (!list) return;
    ListNode *cur = list->head;
    while (cur) {
        ListNode *next = cur->next;
        free(cur);
        cur = next;
    }
    free(list);
}

int list_append(ItemList *list, const Item *item)
{
    ListNode *node = (ListNode *)malloc(sizeof(ListNode));
    if (!node) return -1;
    node->item = *item;
    node->next = NULL;
    node->prev = list->tail;

    if (list->tail)
        list->tail->next = node;
    else
        list->head = node;
    list->tail = node;
    list->count++;
    return 0;
}

ListNode *list_find_by_id(ItemList *list, const char *itemId)
{
    for (ListNode *p = list->head; p; p = p->next) {
        if (strcmp(p->item.itemId, itemId) == 0)
            return p;
    }
    return NULL;
}

int list_remove_by_id(ItemList *list, const char *itemId)
{
    ListNode *node = list_find_by_id(list, itemId);
    if (!node) return -1;

    if (node->prev)
        node->prev->next = node->next;
    else
        list->head = node->next;

    if (node->next)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;

    free(node);
    list->count--;
    return 0;
}

void list_foreach(const ItemList *list,
                  void (*fn)(const Item *item, void *ctx),
                  void *ctx)
{
    if (!list || !fn) return;
    for (ListNode *p = list->head; p; p = p->next)
        fn(&p->item, ctx);
}
