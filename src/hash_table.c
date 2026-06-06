/**
 * @file hash_table.c
 * @brief 哈希表（链地址法）实现
 */
#include <stdlib.h>
#include <string.h>
#include "hash_table.h"

HashTable *hash_create(int capacity)
{
    if (capacity <= 0) capacity = HASH_CAPACITY;//101
    HashTable *t = (HashTable *)malloc(sizeof(HashTable));
    if (!t) return NULL;
    //这里需要把链表首先全部置为NULL，所以用calloc
    t->buckets = (HashNode **)calloc((size_t)capacity, sizeof(HashNode *));
    if (!t->buckets) {
        free(t);
        return NULL;
    }
    t->capacity = capacity;
    t->size = 0;
    return t;
}

void hash_destroy(HashTable *table)
{
    if (!table) return;
    for (int i = 0; i < table->capacity; i++) {
        HashNode *p = table->buckets[i];
        while (p != NULL) {
            HashNode *n = p->next;
            free(p);
            p = n;
        }
    }
    free(table->buckets);
    free(table);
}

//把字符串映射为 [0,capacity - 1]范围的整数，用作下标
unsigned hash_func(const char *key, int capacity)
{
    unsigned h = 0;
    for (int i = 0; key[i] != '\0'; i++)
    //这里的31u为unsigned类型的，防止溢出
        h = h * 31u + (unsigned char)key[i];
    return h % (unsigned)capacity;
}

int hash_insert(HashTable *table, const Item *item)
{
    if (!table || !item) return -1;
    //用哈希映射函数得到下标
    unsigned idx = hash_func(item->itemId, table->capacity);

    //确保当前物品不在哈希表当中
    for (HashNode *p = table->buckets[idx]; p != NULL; p = p->next) {
        if (strcmp(p->item.itemId, item->itemId) == 0) {
            p->item = *item;
            return 0;
        }
    }
    
    //该物品不在哈希表当中，使用头插法插入对应的桶
    HashNode *node = (HashNode *)malloc(sizeof(HashNode));
    if (!node) return -1;
    node->item = *item;
    node->next = table->buckets[idx];
    table->buckets[idx] = node;
    table->size++;
    return 0;
}

//通过唯一的物品id来查找
Item *hash_find(HashTable *table, const char *itemId)
{
    if (!table || !itemId) return NULL;
    unsigned idx = hash_func(itemId, table->capacity);
    for (HashNode *p = table->buckets[idx]; p != NULL; p = p->next) {
        if (strcmp(p->item.itemId, itemId) == 0)
            return &p->item;
    }
    return NULL;
}

int hash_remove(HashTable *table, const char *itemId)
{
    if (!table || !itemId) return -1;
    unsigned idx = hash_func(itemId, table->capacity);
    HashNode *prev = NULL;
    for (HashNode *p = table->buckets[idx]; p != NULL; prev = p, p = p->next) {
        if (strcmp(p->item.itemId, itemId) == 0) {
            if (prev != NULL)
                prev->next = p->next;
            else //这里代表要删除的物品在链表头(因为本项目当中的链表没有设置头节点)
                table->buckets[idx] = p->next;
            free(p);
            table->size--;
            return 0;
        }
    }
    return -1;
}

void hash_foreach(const HashTable *table,
                  void (*fn)(const Item *item, void *ctx),
                  void *ctx)
{
    if (!table || !fn) return;
    //外层控制访问哪一条链表，内层遍历链表内容
    for (int i = 0; i < table->capacity; i++) {
        for (HashNode *p = table->buckets[i]; p != NULL; p = p->next)
            fn(&p->item, ctx);
    }
}
