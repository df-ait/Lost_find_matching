/**
 * @file heap.c
 * @brief 大根堆实现
 */
#include <stdlib.h>
#include <string.h>
#include "heap.h"

//交换两个节点，上浮及下沉都需要交换
static void heap_swap(MatchResult *a, MatchResult *b)
{
    MatchResult t = *a;
    *a = *b;
    *b = t;
}

//上浮，插入以后用，也就是跟父节点比大小，比父节点大就跟父节点交换
static void sift_up(MatchResult *data, int i)
{
    //新元素先放在数组末尾，若比父节点得分高就交换
    //一直往上直到父节点更大或到根
    while (i > 0) {
        //完全二叉树当中，要找父节点下标，就用（子节点下标-1）/ 2
        int p = (i - 1) / 2; 
        if (data[i].score <= data[p].score) break;
        heap_swap(&data[i], &data[p]);
        i = p;
    }
}

//下沉，取顶堆以后用，跟较大的子节点比大小，如果小于子节点，则交换
//传入当前元素下标i，当前堆里还有多少个有效元素n
static void sift_down(MatchResult *data, int n, int i)
{
    //每次取出堆顶元素就执行下沉调整，要重复k次
    while(1){
        int largest = i;//先假设i是最大的
        int l = 2 * i + 1;
        int r = 2 * i + 2;
        if (l < n && data[l].score > data[largest].score){
            largest = l;
        } 
        if (r < n && data[r].score > data[largest].score){
            largest = r;
        } 
        if (largest == i) break;
        heap_swap(&data[i], &data[largest]);
        //交换完以后从孩子的位置继续往下检查
        i = largest;
    }
}

MaxHeap *heap_create(int capacity)
{
    if (capacity <= 0) capacity = MAX_TOP_K * 4;
    MaxHeap *h = (MaxHeap *)malloc(sizeof(MaxHeap));
    if (!h) return NULL;
    h->data = (MatchResult *)malloc((size_t)capacity * sizeof(MatchResult));
    if (!h->data) {
        free(h);
        return NULL;
    }
    h->size = 0;
    h->capacity = capacity;
    return h;
}

void heap_destroy(MaxHeap *heap)
{
    if (!heap) return;
    free(heap->data);
    free(heap);
}

//消除直接将当前已有元素个数清零即可
void heap_clear(MaxHeap *heap)
{
    if (heap) heap->size = 0;
}

int heap_push(MaxHeap *heap, const MatchResult *result)
{
    if (!heap || !result || heap->size >= heap->capacity) return -1;
    heap->data[heap->size] = *result;
    sift_up(heap->data, heap->size);
    heap->size++;
    return 0;
}

int heap_size(const MaxHeap *heap)
{
    return heap ? heap->size : 0;
}

//不懂原堆，复制到tmp上再操作
int heap_top_k(MaxHeap *heap, int k, MatchResult *out, int out_cap)
{
    //k = min{k,out_cap,heap->size} 防止数组越界
    if (!heap || !out || k <= 0) return 0;
    if (k > heap->size) k = heap->size;
    if (k > out_cap) k = out_cap;

    MatchResult *tmp = (MatchResult *)malloc((size_t)heap->size * sizeof(MatchResult));
    if (!tmp) return 0;
    memcpy(tmp, heap->data, (size_t)heap->size * sizeof(MatchResult));
    int n = heap->size;

    for (int i = 0; i < k; i++) {
        out[i] = tmp[0];//堆顶，当前最大
        tmp[0] = tmp[n - 1];//取出堆顶后将最后一个元素放到堆顶，n--，进行下沉
        n--;
        sift_down(tmp, n, 0);
    }
    free(tmp);
    return k;
}
