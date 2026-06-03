#ifndef TYPES_H
#define TYPES_H

#include <time.h>

/* ---------- 字符串长度 ---------- */
#define NAME_LEN        64  //物品/失主名称 最大长度
#define LOC_LEN         64
#define DESC_LEN        256 //外观
#define SECRET_LEN      256
#define PHONE_LEN       20
#define ID_LEN          32
#define CATEGORY_LEN    32 //类别

#define MAX_TOP_K       10  //寻物最多展示前X条结果
#define HASH_CAPACITY   101 //哈希表桶数量，这里用指数以减少冲突
#define MATCH_THRESHOLD 60.0    //最低匹配分，低于这个分不让领失物
#define TIME_WINDOW_SEC (2 * 3600)   //精确寻物默认 ±2 小时

//---------- 物品状态 ---------- 
typedef enum {
    ITEM_IN_STORAGE = 0,//在库
    ITEM_CLAIMED,   //已领走
    ITEM_ARCHIVED   //长期无人领取
} ItemStatus;

//---------- 认领状态 ----------
typedef enum {
    CLAIM_PENDING = 0,  //待审核
    CLAIM_APPROVED,     //通过
    CLAIM_REJECTED      //拒绝
} ClaimStatus;

//---------- 物品（拾物入库记录） ----------
typedef struct Item {
    char itemId[ID_LEN];    //作为哈希表主键，唯一编号
    char name[NAME_LEN];
    char category[CATEGORY_LEN];
    time_t foundTime;              //交到招领处时间
    char location[LOC_LEN];
    char description[DESC_LEN];
    char secretFeatures[SECRET_LEN]; //保密，列表不展示
    ItemStatus status;
    time_t registerTime;//系统登记时间
} Item;

// ---------- 寻物登记（失主查找条件） ----------
typedef struct LostReport {
    char reportId[ID_LEN];//寻物单号[非必须]
    char itemName[NAME_LEN];
    char category[CATEGORY_LEN];
    //丢失物品区间
    time_t lostTimeStart;
    time_t lostTimeEnd;

    char lostLocation[LOC_LEN];
    char ownerName[NAME_LEN];
    char phone[PHONE_LEN];
    char description[DESC_LEN];
} LostReport;

//---------- 认领申请 ---------
typedef struct ClaimRequest {
    char claimId[ID_LEN];//认领单号
    char itemId[ID_LEN];
    char ownerName[NAME_LEN];
    char phone[PHONE_LEN];
    char answerSecret[SECRET_LEN];
    double matchScore; //匹配得分
    ClaimStatus status; //审核结果
} ClaimRequest;

//---------- 匹配结果（堆元素） ----------
typedef struct MatchResult {
    char itemId[ID_LEN];
    double score;
    Item item;      //物品完整副本               
} MatchResult;

#endif /* TYPES_H */
