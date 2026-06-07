//物品管理 — 登记、查询、全局索引
#ifndef ITEM_MGR_H
#define ITEM_MGR_H

#include "types.h"
#include "list.h"
#include "hash_table.h"
#include "bst.h"

//物品仓库
typedef struct {
    HashTable *by_id;
    ItemList  *all_items;
    BstNode   *by_time;
    int        id_counter;
} ItemStore;

//创建物品仓库
ItemStore *store_create();
//销毁
void       store_destroy(ItemStore *store);
//物品注册(有新的失物)
int        store_register_item(ItemStore *store, const Item *item);
//通过ID找物品
Item      *store_find_by_id(ItemStore *store, const char *itemId);
//审核通过与否
int        store_set_status(ItemStore *store, const char *itemId, ItemStatus status);
//浏览库中失物
void       store_browse_in_storage(const ItemStore *store);
//生成物品id
void       generate_item_id(char *buf, int id_num);
//生成寻物id
void       generate_claim_id(char *buf, int id_num);

#endif /* ITEM_MGR_H */
