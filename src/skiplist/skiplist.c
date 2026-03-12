

#include "skiplist.h"
#include <stdlib.h>
#include <stdio.h>

/** @brief 跳跃表最大层数，决定插入的最大随机层 */
#define ZSKIPLIST_MAXLEVEL 32

/** @brief 跳跃表层级提升概率 = 1/4，控制层级分布 */
#define ZSKIPLIST_P 0.25

/**
 * @brief 创建一个新的跳跃表节点
 * 分配 skiplist_node_t + level 个 skiplistLevel 结构的连续内存。
 * @param level 节点层数（1 ~ ZSKIPLIST_MAXLEVEL）
 * @param score 节点关联值（用于排序）
 * @param ele   节点键（SDS 字符串）
 * @return skiplist_node_t* 新节点指针
 */
skiplist_node_t* skipListCreateNode(int level, void* score, sds_t ele) {
    /* 分配节点主体 + 柔性数组中各层的空间 */
    skiplist_node_t* node = zmalloc(sizeof(skiplist_node_t) + sizeof(skiplist_node_t) * level);
    node->backward = NULL;
    node->ele = ele;
    node->score = score;
    /* 初始化各层的前向指针和跨度 */
    for (int i = 0; i < level; i++) {
        node->level[i].forward = NULL;
        node->level[i].span = 0;
    }
    return node;
}

/**
 * @brief 释放一个跳跃表节点，返回其存储的值
 * 释放节点的 SDS 键和节点本身，调用方可通过返回值处理节点的 score。
 * @param node 要释放的节点
 * @return void* 节点存储的 score 值指针
 */
void* skipListFreeNode(skiplist_node_t* node) {
    void* score = node->score; /* 保存 score 供调用方使用 */
    sds_delete(node->ele);     /* 释放 SDS 键 */
    zfree(node);
    return score;
}

/**
 * @brief 释放整个跳跃表及所有节点
 * 从第 0 层正向遍历所有节点，逐一释放并调用 free_value 回调。
 * @param sl         目标跳跃表
 * @param free_value 节点值的释放回调函数
 */
void skipListFree(skiplist_t* sl, freeValue* free_value) {
    /* 从 header 的第 0 层第一个真实节点开始 */
    skiplist_node_t *node = sl->header->level[0].forward, *next;
    zfree(sl->header); /* 释放哨兵节点 */
    while(node) {
        next = node->level[0].forward;
        void* v = skipListFreeNode(node); /* 释放节点，得到 score */
        free_value(v);                    /* 调用方负责释放 score */
        node = next;
    }
    zfree(sl);
}

/**
 * @brief 生成随机层数（幂律分布）
 * 每层以 ZSKIPLIST_P 的概率继续提升，保证高层节点数量指数级减少。
 * @return int 生成的随机层数（范围：1 ~ ZSKIPLIST_MAXLEVEL）
 */
int skipListRandomLevel(void) {
    int level = 1;
    /* 以 ZSKIPLIST_P 概率继续提升层数 */
    while ((random()&0xFFFF) < (ZSKIPLIST_P * 0xFFFF))
        level += 1;
    return (level<ZSKIPLIST_MAXLEVEL) ? level : ZSKIPLIST_MAXLEVEL;
}

/**
 * @brief 创建并初始化一个新跳跃表
 * header 为哨兵节点（含 MAXLEVEL 层，不存储实际数据）。
 * @param c 节点比较函数指针
 * @return skiplist_t* 新建跳跃表指针
 */
skiplist_t* skipListNew(comparator* c) {
    int j;
    skiplist_t* sl;
    sl = (skiplist_t*)zmalloc(sizeof(skiplist_t));
    sl->level = 1;   /* 初始层数为 1 */
    sl->length = 0;
    /* 创建哨兵 header 节点：score=0, ele=NULL */
    sl->header = skipListCreateNode(ZSKIPLIST_MAXLEVEL, 0, NULL);
    for (j = 0; j < ZSKIPLIST_MAXLEVEL; j++) {
        sl->header->level[j].forward = NULL;
        sl->header->level[j].span = 0;
    }
    sl->header->backward = NULL;
    sl->tail = NULL;
    sl->comparator = c;
    return sl;
}

/**
 * @brief 向跳跃表插入一个节点
 * 算法：
 *   1. 从最高层向下查找每层的插入前驱节点，记录在 update[] 中，
 *      同时累计每层的 rank（用于计算 span）
 *   2. 生成随机层数；若新层数超过当前最大层，补充 update[]
 *   3. 创建新节点，按层更新前向指针和 span
 *   4. 更新 backward 指针和 tail
 * @param sl    目标跳跃表
 * @param score 节点关联值（排序依据）
 * @param ele   节点键（SDS 字符串，由节点持有）
 * @return skiplist_node_t* 插入的新节点指针
 */
skiplist_node_t* skipListInsert(skiplist_t *sl, void* score, sds_t ele) {
    skiplist_node_t *update[ZSKIPLIST_MAXLEVEL], *x; /* update[i]=第i层插入前驱 */
    int rank[ZSKIPLIST_MAXLEVEL];                    /* rank[i]=到达update[i]的累计跨度 */
    int i, level;
    x = sl->header;

    /* 从高层到低层，寻找每层插入位置的前驱节点 */
    for(i = sl->level-1; i >= 0; i--) {
        rank[i] = i == (sl->level - 1) ? 0: rank[i+1];
        while (x->level[i].forward &&
            sl->comparator(x->level[i].forward->ele,
                x->level[i].forward->score,
                ele,
                score) < 0) {
            rank[i] += x->level[i].span;
            x = x->level[i].forward;
        }
        update[i] = x; /* 记录第 i 层的前驱节点 */
    }
    level = skipListRandomLevel(); /* 新节点随机层数 */
    if (level > sl->level) {
        /* 新层数超过当前最大层，对超出部分以 header 为前驱，span 设为 sl->length */
        for (i = sl->level; i < level; i++) {
            rank[i] = 0;
            update[i] = sl->header;
            update[i]->level[i].span = sl->length;
        }
        sl->level = level;
    }
    x = skipListCreateNode(level, score, ele);
    /* 逐层插入：更新前向指针和 span */
    for (i = 0; i < level; i++) {
        x->level[i].forward = update[i]->level[i].forward;
        update[i]->level[i].forward = x;
        /* span = 前驱原 span - (rank[0]-rank[i])，表示新节点到下一节点的跨度 */
        x->level[i].span = update[i]->level[i].span - (rank[0] - rank[i]);
        update[i]->level[i].span = (rank[0] - rank[i]) + 1;
    }

    /* 未覆盖的高层，前驱的 span 要加 1（因为插入了新节点） */
    for (i = level; i < sl->level; i++) {
        update[i]->level[i].span++;
    }
    /* 更新 backward 反向指针 */
    x->backward = (update[0] == sl->header)? NULL: update[0];
    if (x->level[0].forward) {
        x->level[0].forward->backward = x;
    } else {
        sl->tail = x; /* 新节点是最后一个节点 */
    }
    sl->length++;
    return x;
}

/**
 * @brief 从跳跃表中删除指定节点（内部辅助函数）
 * 更新各层的前向指针和 span，维护 backward 指针和 tail，调整最大层数。
 * @param sl     目标跳跃表
 * @param x      要删除的节点
 * @param update 各层的前驱节点数组（由调用方查找后传入）
 */
void skiplistDeleteNode(skiplist_t *sl, skiplist_node_t *x, skiplist_node_t **update) {
    int i;
    for (i = 0; i < sl->level; i++) {
        if (update[i]->level[i].forward == x) {
            /* 将 x 的 span 合并到前驱，前驱直接跳过 x */
            update[i]->level[i].span += x->level[i].span - 1;
            update[i]->level->forward = x->level[i].forward;
        } else {
            /* x 不在此层，但前驱的 span 要减 1 */
            update[i]->level[i].span -= 1;
        }
    }

    /* 维护 backward 指针和 tail */
    if (x->level[0].forward) {
        x->level[0].forward->backward = x->backward;
    } else {
        sl->tail = x->backward; /* 被删节点是 tail，更新 tail */
    }
    /* 收缩最大层数（高层为空则降层） */
    while(sl->level > 1 && sl->header->level[sl->level-1].forward == NULL) {
        sl->level--;
    }
    sl->length--;
}

/**
 * @brief 从跳跃表中查找并删除匹配节点
 * 从高层到低层定位，找到 score 和 ele 均匹配的节点后调用 skiplistDeleteNode。
 * @param sl   目标跳跃表
 * @param score 要删除节点的值
 * @param ele   要删除节点的键
 * @param node  输出参数，不为 NULL 时将被删节点指针写入（调用方负责释放）
 * @return void* 被删节点的 score，未找到返回 NULL
 */
void* skipListDelete(skiplist_t *sl, void* score, sds_t ele, skiplist_node_t **node) {
    skiplist_node_t *update[ZSKIPLIST_MAXLEVEL], *x;
    int i;

    x = sl->header;
    /* 从高层向低层查找前驱节点 */
    for(i = sl->level-1; i >= 0; i--) {
        while(x->level[i].forward &&
            sl->comparator(x->level[i].forward->ele,
                x->level[i].forward->score,
                ele, score) < 0) {
            x = x->level[i].forward;
        }
        update[i] = x;
    }

    x = x->level[0].forward; /* 候选节点 */
    /* 验证 score 和 ele 是否完全匹配 */
    if (x && sl->comparator(NULL, x->score, NULL, score) == 0
        && sds_cmp(x->ele, ele) == 0) {
        skiplistDeleteNode(sl, x, update);
        if (!node) {
            return skipListFreeNode(x); /* node=NULL 时直接释放节点 */
        } else {
            *node = x; /* node!=NULL 时将节点指针输出给调用方 */
            return x->score;
        }
    }
    return NULL; /* 未找到 */
}


int slValueGteMin(skiplist_t* sl, void* score, zrangespec* zs) {
    int result = sl->comparator(NULL, score, NULL, zs->min);
    return zs->minex ? result > 0: result >= 0;
}

int slValueLteMax(skiplist_t* sl, void* score, zrangespec* zs) {
    int result = sl->comparator(NULL, score, NULL, zs->max);
    return zs->maxex ? result < 0: result <= 0;
}


int slIsInRange(skiplist_t* sl, zrangespec *range) {
    skiplist_node_t *x;
    int result = sl->comparator(NULL, range->min, NULL, range->max);

    if (result > 0 || 
            (result == 0 && (range->minex || range->maxex))) {
        return 0;
    }
    x = sl->tail;
    if (x == NULL || !slValueGteMin(sl, x->score, range)) 
        return 0;
    x = sl->header->level[0].forward;
    if ( x == NULL || !slValueLteMax(sl, x->score, range))
        return 0;
    return 1;
}

skiplist_node_t* skiplist_firstInRange(skiplist_t* sl, zrangespec* range) {
    skiplist_node_t* x;
    int i;

    if (!slIsInRange(sl, range)) {
        return NULL;
    }
    x = sl->header;
    for( i = sl->level-1; i >= 0; i-- ) {
        while(x->level[i].forward &&
            !slValueGteMin(sl, x->level[i].forward->score, range)) {
            x = x->level[i].forward;
        }
    }
    
    x = x->level[0].forward;
    // assert(x != NULL);
    if (!slValueLteMax(sl, x->score, range)) return NULL;

    return x;
}

skiplist_node_t* skiplist_lastInRange(skiplist_t *sl, zrangespec* range) {
    skiplist_node_t *x;
    int i;

    if (!slIsInRange(sl, range)) return NULL;

    x = sl->header;
    for (i = sl->level-1; i >= 0; i--) {
        while (x->level[i].forward &&
            slValueLteMax(sl, x->level[i].forward->score, range))
                x = x->level[i].forward;
    }
    //assert(x != NULL);

    if (!slValueGteMin(sl, x->score, range)) {
        return NULL;
    }
    return x;
}

void slDeleteNode(skiplist_t *zsl, skiplist_node_t *x, skiplist_node_t **update) {
    int i;
    for (i = 0; i < zsl->level; i++) {
        if (update[i]->level[i].forward == x) {
            update[i]->level[i].span += x->level[i].span - 1;
            update[i]->level[i].forward = x->level[i].forward;
        } else {
            update[i]->level[i].span -= 1;
        }
    }
    if (x->level[0].forward) {
        x->level[0].forward->backward = x->backward;
    } else {
        zsl->tail = x->backward;
    }
    while(zsl->level > 1 && zsl->header->level[zsl->level-1].forward == NULL)
        zsl->level--;
    zsl->length--;
}

/* Free the specified skiplist node. The referenced SDS string representation
 * of the element is freed too, unless node->ele is set to NULL before calling
 * this function. */
void zslFreeNode(skiplist_t* sl, skiplist_node_t *node) {
    sds_delete(node->ele);
    zfree(node);
}

