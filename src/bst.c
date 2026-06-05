#include <stdlib.h>
#include <string.h>
#include "bst.h"

static BstNode *bst_node_new(const Item *item)
{
    BstNode *n = (BstNode *)malloc(sizeof(BstNode));
    if (!n) return NULL;
    n->item = *item;
    n->left = n->right = NULL;
    return n;
}

BstNode *bst_insert(BstNode *root, const Item *item)
{
    if (!item) return root;
    if (!root) return bst_node_new(item);

    if (item->foundTime < root->item.foundTime){
        //小的(时间更早的)，放左子树，用递归
        root->left = bst_insert(root->left, item);
    }
    else if (item->foundTime > root->item.foundTime){
        root->right = bst_insert(root->right, item);
    }
    else {
        //时间相同就按itemId字典序放
        /*strcmp(S1,S2)函数本质就是把字符串中每个字符的ASCII码值进行相减
          遇到ASCII码值不一样的字符就用  S1对应字符 - S2对应字符，可以根据返回值来判断字典序大小
        */
        if (strcmp(item->itemId, root->item.itemId) <= 0){
            //字典序小的去左子树
            root->left = bst_insert(root->left, item);
        } 
        else{
            //字典树大的去右子树
            root->right = bst_insert(root->right, item);
        }
    }
    return root;
}

BstNode *bst_find_by_id(BstNode *root, const char *itemId)
{
    //查找顺序：中 -> 左 -> 右
    if (!root || !itemId) return NULL;
    int c = strcmp(itemId, root->item.itemId);
    if (c == 0) return root;
    //递归找左子树，右子树
    BstNode *l = bst_find_by_id(root->left, itemId);
    if (l != NULL) return l;
    return bst_find_by_id(root->right, itemId);
}

//在BTS里面找出所有 foundTime落在区间 [t0,t1]的物品，放入out链表
//作为对内实现细节，static关键字保证只在本.c文件能调用
static void bst_collect_range(BstNode *root, time_t t0, time_t t1, ItemList *out)
{
    if (!root || !out) return;
    if (root->item.foundTime > t1){
        //根节点时间过晚，要找更早的时间
        bst_collect_range(root->left, t0, t1, out);
    } 
    if (root->item.foundTime >= t0 && root->item.foundTime <= t1){
        list_append(out, &root->item);
        //append根节点以后还要看他的子节点符不符合，如果不加后面这两句的话，符合条件的节点的子节点就永远不会被访问
        bst_collect_range(root->left, t0, t1, out);
        bst_collect_range(root->right, t0, t1, out);
    }
    if (root->item.foundTime < t0){
        //根节点时间过早，需要找更晚的时间
        bst_collect_range(root->right, t0, t1, out);
    }       
}

//作为对外接口
void bst_range_query(BstNode *root, time_t t0, time_t t1, ItemList *out)
{
    bst_collect_range(root, t0, t1, out);
}

//按照中序遍历顺序
void bst_inorder_foreach(BstNode *root,
                         void (*fn)(const Item *item, void *ctx),
                         void *ctx)
{
    if (!root || !fn) return;
    bst_inorder_foreach(root->left, fn, ctx);
    fn(&root->item, ctx);
    bst_inorder_foreach(root->right, fn, ctx);
}

void bst_free(BstNode *root)
{
    //按照左中右顺序free
    if (!root) return;
    bst_free(root->left);
    bst_free(root->right);
    free(root);
}
