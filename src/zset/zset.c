#include "zset.h"

/**
 * @brief 创建一个跳跃表节点（内部辅助函数）
 * 分配节点结构体 + level 个 zskip_list_level_t 柔性数组元素。
 * @param level 节点的层数
 * @param score 节点分数
 * @param ele   节点键（SDS 字符串）
 * @return zskip_list_node_t* 新建节点指针
 */
zskip_list_node_t* zskip_list_node_new(int level, double score, sds ele) {
    zskip_list_node_t *zn =
        zmalloc(sizeof(*zn)+level*sizeof(struct zskip_list_level_t));
    zn->score = score;
    zn->ele = ele;
    return zn;
}

/**
 * @brief 创建一个新的跳跃表
 * 分配 zskip_list_t，创建 ZSKIPLIST_MAXLEVEL 层的哨兵头节点，
 * 初始化所有层的 forward 和 span，tail 置 NULL，初始 level=1。
 * @return zskip_list_t* 新建跳跃表指针
 */
zskip_list_t* zskip_list_new() {
    int j;
    zskip_list_t* zsl;
    zsl = zmalloc(sizeof(*zsl));
    zsl->level = 1;
    zsl->length = 0;
    /* 创建哨兵头节点（最大层数），不存储实际数据 */
    zsl->header = zskip_list_node_new(ZSKIPLIST_MAXLEVEL, 0, NULL);
    for (j = 0; j < ZSKIPLIST_MAXLEVEL; j++) {
        zsl->header->level[j].forward = NULL;
        zsl->header->level[j].span = 0;
    }
    zsl->header->backward = NULL;
    zsl->tail = NULL;
    return zsl;
}

/**
 * @brief 创建一个新的有序集合
 * 分配 zset_t，创建跳跃表，使用外部传入的 dict（不拷贝）。
 * @param dict 外部哈希表（存储 key → score 映射）
 * @return zset_t* 新建有序集合指针
 */
zset_t* zset_new(dict_t* dict) {
    zset_t* z = zmalloc(sizeof(zset_t));
    z->zsl = zskip_list_new();
    z->dict = dict;
    return z;
}

/**
 * @brief 释放一个跳跃表节点（内部辅助函数）
 * 释放节点键（SDS）和节点结构体本身。
 * @param node 目标节点指针
 */
void zsl_free_node(zskip_list_node_t* node) {
    sds_delete(node->ele); /* 释放键字符串 */
    zfree(node);
}

/**
 * @brief 释放整个跳跃表（含所有节点）
 * 先释放哨兵头节点，再沿 level[0].forward 链遍历释放所有真实节点。
 * @param zsl 目标跳跃表指针
 */
void zsl_free(zskip_list_t* zsl) {
    zskip_list_node_t* node = zsl->header->level[0].forward, *next;
    zfree(zsl->header); /* 释放哨兵节点 */
    while(node) {
        next = node->level[0].forward;
        zsl_free_node(node);
        node = next;
    }
    zfree(zsl);
}

/**
 * @brief 销毁有序集合：释放 dict、跳跃表和结构体本身
 * @param zs 目标有序集合指针
 */
void zset_delete(zset_t* zs) {
    dict_delete(zs->dict);  /* 释放哈希表 */
    zsl_free(zs->zsl);      /* 释放跳跃表 */
    zfree(zs);
}
