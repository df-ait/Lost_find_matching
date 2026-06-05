//二叉搜索树，按 foundTime 支持时间范围查询
//小的左边，大的右边
#ifndef BST_H
#define BST_H

#include "types.h"
#include "list.h"

//每个节点弄一个左右孩子指针方便比较大小
typedef struct BstNode {
    Item item;
    struct BstNode *left;
    struct BstNode *right;
} BstNode;

//插入节点
BstNode *bst_insert(BstNode *root, const Item *item);
//通过ID查找物品
BstNode *bst_find_by_id(BstNode *root, const char *itemId);
//按照时间范围查找物品
void bst_range_query(BstNode *root, time_t t0, time_t t1, ItemList *out);
//遍历
void bst_inorder_foreach(BstNode *root,
                             void (*fn)(const Item *item, void *ctx),
                             void *ctx);
//释放空间
void bst_free(BstNode *root);

#endif /* BST_H */
