/*
 * lfu_cache.h - LFU 缓存头文件
 * 
 * Latte C 库缓存组件：基于 LFU 算法的缓存实现
 * 
 * 核心特性：
 * 1. LFU (最不经常使用) 淘汰策略
 * 2. 基于访问频率而非时间
 * 3. 自动淘汰访问次数最少的项
 * 4. 使用 dict + 频率桶实现高效操作
 * 
 * 与 LRU 的区别：
 * - LRU: 淘汰最久未使用的项 (基于时间)
 * - LFU: 淘汰访问最少的项 (基于频率)
 * 
 * 适用场景：
 * - 热点数据访问模式明显
 * - 数据访问频率差异大
 * - 需要长期保留热门数据
 * 
 * 数据结构：
 * - dict: key -> entry 映射 (O(1) 查找)
 * - freq_buckets: 频率桶数组 (按频率分组)
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef __LATTE_LFU_CACHE_H
#define __LATTE_LFU_CACHE_H

#include "list/list.h"
#include "dict/dict.h"

/* LFU 缓存条目 */
typedef struct lfu_cache_entry_t {
    void *key;
    void *value;
    uint32_t frequency;       /* 访问频率计数 */
    uint64_t last_access;     /* 上次访问时间 (用于同频率时的 tie-break) */
} lfu_cache_entry_t;

/* LFU 缓存结构 */
typedef struct lfu_cache_t {
    dict_t *items;            /* key -> entry 映射 */
    list_t **freq_buckets;    /* 频率桶数组 */
    uint32_t min_frequency;   /* 当前最小频率 */
    uint32_t max_frequency;   /* 当前最大频率 */
    uint32_t capacity;        /* 最大容量 */
    uint32_t size;            /* 当前大小 */
    uint64_t now;             /* 当前时间 (用于模拟) */
} lfu_cache_t;

/* LFU 回调函数集 */
typedef struct lfu_cache_func_t {
    dict_func_t super;
    uint32_t (*decay_fn)(uint32_t freq);  /* 频率衰减函数 (可选) */
} lfu_cache_func_t;

/* 创建 LFU 缓存
 * 参数：func - 回调函数集 (可选 NULL)
 *       capacity - 最大容量
 * 返回：成功返回缓存指针，失败返回 NULL
 */
lfu_cache_t* lfu_cache_new(lfu_cache_func_t *func, uint32_t capacity);

/* 删除 LFU 缓存
 * 参数：cache - 待删除的缓存
 */
void lfu_cache_delete(lfu_cache_t *cache);

/* 获取缓存项 (同时增加频率)
 * 参数：cache - 缓存指针
 *       key - 键
 * 返回：找到返回值指针，未找到返回 NULL
 */
void* lfu_cache_get(lfu_cache_t *cache, void *key);

/* 放入缓存项
 * 参数：cache - 缓存指针
 *       key - 键
 *       val - 值
 * 返回：成功返回 0，失败返回 -1
 * 注意：如果容量已满，淘汰频率最低的项
 */
int lfu_cache_put(lfu_cache_t *cache, void *key, void *val);

/* 移除缓存项
 * 参数：cache - 缓存指针
 *       key - 键
 */
void lfu_cache_remove(lfu_cache_t *cache, void *key);

/* 获取缓存大小
 * 参数：cache - 缓存指针
 * 返回：当前条目数量
 */
uint32_t lfu_cache_size(lfu_cache_t *cache);

/* 获取缓存容量
 * 参数：cache - 缓存指针
 * 返回：最大容量
 */
uint32_t lfu_cache_capacity(lfu_cache_t *cache);

/* 手动降低所有项的频率 (用于周期性衰减)
 * 参数：cache - 缓存指针
 */
void lfu_cache_decay(lfu_cache_t *cache);

#endif /* __LATTE_LFU_CACHE_H */
