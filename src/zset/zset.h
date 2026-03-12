
#ifndef __LATTE_ZSET_H
#define __LATTE_ZSET_H
#include "dict/dict.h"
#include "sds/sds.h"

/** @brief 跳跃表最大层数（支持 2^64 个元素） */
#define ZSKIPLIST_MAXLEVEL 32
/** @brief 跳跃表随机层概率因子（P = 1/4） */
#define ZSKIPLIST_P 0.25

/**
 * @brief 有序集合跳跃表节点
 *
 * 每个节点存储键（ele）、分数（score）、反向指针和多层前向指针。
 */
typedef struct zskip_list_node_t {
    sds ele;                            /**< 节点键（SDS 字符串） */
    double score;                       /**< 排序分数（double 类型） */
    struct zskip_list_node_t *backward; /**< 第 0 层反向指针（用于逆向遍历） */
    struct zskip_list_level_t {
        struct zskip_list_node_t* forward;  /**< 该层前向指针 */
        unsigned long span;                 /**< 该层跨越的节点数（用于 rank 计算） */
    } level[];                          /**< 每层的指针和跨度（柔性数组） */
} zskip_list_node_t;

/**
 * @brief 有序集合跳跃表
 * 维护 header（哨兵）和 tail 指针，以及长度和当前最高层数。
 */
typedef struct zskip_list_t {
    struct zskip_list_node_t* header;   /**< 哨兵头节点（不存储实际数据） */
    struct zskip_list_node_t* tail;     /**< 最后一个真实节点 */
    unsigned long length;               /**< 实际节点数（不含 header） */
    int level;                          /**< 当前最高层数 */
} zskip_list_t;

/**
 * @brief 有序集合（ZSet）结构体
 *
 * 双索引结构：dict 支持 O(1) 按键查找分数，zsl 支持 O(log N) 按分数范围查询。
 */
typedef struct zset_t {
    dict_t* dict;       /**< 哈希表：键 → 分数（O(1) 按键查找） */
    zskip_list_t *zsl;  /**< 跳跃表：按分数排序（O(log N) 范围查询） */
} zset_t;

/**
 * @brief 创建一个新的跳跃表
 * @return zskip_list_t* 新建跳跃表指针
 */
zskip_list_t* zskip_list_new();

/**
 * @brief 创建一个新的有序集合
 * @param dict 外部提供的哈希表（用于按键查找分数）
 * @return zset_t* 新建有序集合指针
 */
zset_t* zset_new(dict_t* dict);

/**
 * @brief 向有序集合添加或更新键-分数对
 * @param zset  目标有序集合
 * @param key   键指针
 * @param score 排序分数
 * @return int 成功返回 1，失败返回 0
 */
int zset_add(zset_t* zset, void* key, double score);

/**
 * @brief 从有序集合删除指定键
 * @param zset 目标有序集合
 * @param key  要删除的键指针
 * @return int 成功返回 1，键不存在返回 0
 */
int zset_remove(zset_t* zset, void* key);

/**
 * @brief 释放有序集合及其内部跳跃表（不释放 dict）
 * @param zset 目标有序集合指针
 */
void zset_delete(zset_t* zset);

#endif
