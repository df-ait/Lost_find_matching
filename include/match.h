// 匹配引擎 — 评分、精确/模糊寻物
#ifndef MATCH_H
#define MATCH_H

#include "types.h"
#include "heap.h"
#include "item_mgr.h"

typedef struct {
    const LostReport *report;
    MaxHeap *heap;
} FuzzyCtx;


//计算匹配分数
double calc_match_score(const Item *item, const LostReport *report);
//登记时是否录入了有效保密特征
int item_has_secret(const Item *item);
//匹配秘密信息
int verify_secret(const Item *item, const char *answer);
//精确寻物
int match_precise(ItemStore *store, const LostReport *report,
                     MaxHeap *heap, int top_k);
//模糊寻物
int match_fuzzy(ItemStore *store, const LostReport *report,
                   MaxHeap *heap, int top_k);
//打印出候选物品
void print_match_results(const MatchResult *results, int count);

#endif /* MATCH_H */
