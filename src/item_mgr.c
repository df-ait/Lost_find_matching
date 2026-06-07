//物品存储与登记管理
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "item_mgr.h"

ItemStore *store_create()
{
    ItemStore *s = (ItemStore *)malloc(sizeof(ItemStore));
    if (!s) return NULL;
    s->by_id = hash_create(HASH_CAPACITY);
    s->all_items = list_create();
    s->by_time = NULL;
    s->id_counter = 0;
    if (!s->by_id || !s->all_items) {
        store_destroy(s);
        return NULL;
    }
    return s;
}

void store_destroy(ItemStore *store)
{
    if (!store) return;
    hash_destroy(store->by_id);
    list_destroy(store->all_items);
    bst_free(store->by_time);
    free(store);
}

//生成格式为LF00000001的ID，这是登记前使用
void generate_item_id(char *buf, int id_num)
{
    snprintf(buf, ID_LEN, "LF%08d", id_num);
}

//生成格式为CL00000001的ID，生成认领单的时候用
void generate_claim_id(char *buf, int id_num)
{
    snprintf(buf, ID_LEN, "CL%08d", id_num);
}

//插入物品，往哈希表，链表，二叉搜索树都插入
//任何一步失败了都会直接返回
int store_register_item(ItemStore *store, const Item *item)
{
    if (!store || !item) return -1;
    if (hash_insert(store->by_id, item) != 0) return -1;
    if (list_append(store->all_items, item) != 0) return -1;
    store->by_time = bst_insert(store->by_time, item);
    if (!store->by_time) return -1;
    return 0;
}

//直接在哈希表当中通过物品ID查找
Item *store_find_by_id(ItemStore *store, const char *itemId)
{
    if (!store) return NULL;
    return hash_find(store->by_id, itemId);
}

//三份内容都要一样，状态必须同步，不然会矛盾
int store_set_status(ItemStore *store, const char *itemId, ItemStatus status)
{
    Item *it = store_find_by_id(store, itemId);
    if (!it) return -1;
    it->status = status;

    ListNode *node = list_find_by_id(store->all_items, itemId);
    if (node != NULL) node->item.status = status;

    BstNode *bn = bst_find_by_id(store->by_time, itemId);
    if (bn != NULL) bn->item.status = status;

    return 0;
}

//打印当前物品状态
static void print_item_row(const Item *item, void *ctx)
{
    const char *st = "在库";
    if (item->status == ITEM_CLAIMED) st = "已认领";
    else if (item->status == ITEM_ARCHIVED) st = "已归档";

    char tbuf[32];
    struct tm *tm_info = localtime(&item->foundTime);
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M", tm_info);

    printf("  %-14s %-12s %-10s %-16s %s\n",
           item->itemId, item->name, item->category, tbuf, st);
    printf("----地点: %s\n", item->location);
    printf("----描述: %s\n", item->description);
}

//打印仓库内容
void store_browse_in_storage(const ItemStore *store)
{
    if (!store) return;
    printf("\n  %-14s %-12s %-10s %-16s %s\n",
           "编号", "名称", "类别", "交到招领处时间", "状态");
    printf("  %s\n", "----------------------------------------------------------------");

    for (ListNode *p = store->all_items->head; p; p = p->next) {
        if (p->item.status == ITEM_IN_STORAGE)
            print_item_row(&p->item, NULL);
    }
}
