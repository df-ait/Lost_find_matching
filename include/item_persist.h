#ifndef ITEM_PERSIST_H
#define ITEM_PERSIST_H

#include "item_mgr.h"

//默认数据文件相对路径
#define ITEM_PERSIST_PATH "data/items.txt"

//从文件加载在库物品；文件不存在视为空库，返回0
int store_load_items(ItemStore *store, const char *path);

//将全部物品（含已认领）写回文件
int store_save_items(const ItemStore *store, const char *path);

#endif /* ITEM_PERSIST_H */
