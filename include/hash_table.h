//链地址法的哈希，冲突就接到链表后面
#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "types.h"

typedef struct HashNode {
    Item item;
    struct HashNode *next;
} HashNode;

//这里buckets存储的是每个链表的头节点
typedef struct {
    HashNode **buckets;
    int capacity;//代表buckets存了多少个链表头，也就是数组长度
    int size;//当前哈希表实际存了多少个数据
} HashTable;

//创建哈希表
HashTable *hash_create(int capacity);
void       hash_destroy(HashTable *table);
//哈希函数进行映射
unsigned   hash_func(const char *key, int capacity);
//插入
int        hash_insert(HashTable *table, const Item *item);
//寻找物品
Item      *hash_find(HashTable *table, const char *itemId);
//移除物品
int        hash_remove(HashTable *table, const char *itemId);
//遍历
void       hash_foreach(const HashTable *table,
                        void (*fn)(const Item *item, void *ctx),
                        void *ctx);

#endif /* HASH_TABLE_H */
