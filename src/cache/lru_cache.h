/*
 * lru_cache.h - LRU 缓存头文件
 * 
 * Latte C 库缓存组件：基于 LRU 算法的缓存实现
 * 
 * 核心特性：
 * 1. LRU (最近最少使用) 淘汰策略
 * 2. O(1) 时间复杂度 get/put
 * 3. 自动淘汰最久未使用的项
 * 4. 使用 dict+list 组合实现高效查找和顺序维护
 * 
 * 数据结构：
 * - dict: 快速查找 key -> list_node 映射 (O(1))
 * - list: 维护使用顺序 (最近使用在头，最久未用在尾)
 * 
 * 操作流程：
 * - get: 查找 dict -> 移动节点到 list 头
 * - put: 查找 dict -> 更新/插入节点到 list 头
 * - 满时：删除 list 尾节点 + dict 对应项
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef __LATTE_LRU_CACHE_H
#define __LATTE_LRU_CACHE_H

#include "list/list.h"
#include "dict/dict.h"

/* LRU 缓存结构
 * 使用 dict + list 组合实现 O(1) 操作
 */
typedef struct lru_cache_t { 
    list_t* list;         /* 双向链表：维护使用顺序 (头=最近使用，尾=最久未使用) */
    dict_t* searcher;     /* 哈希表：key -> list_node 映射，实现 O(1) 查找 */
} lru_cache_t;

/* LRU 回调函数集
 * 继承 dict_func_t，添加缓存特定功能
 */
typedef struct lru_cache_func_t {
    dict_func_t super;    /* 父类：dict 回调函数 (hash/dup/compare/destructor) */
} lru_cache_func_t;

/* 创建 LRU 缓存
 * 参数：func - 回调函数集 (可选 NULL，使用默认)
 * 返回：成功返回缓存指针，失败返回 NULL
 */
lru_cache_t* lru_cache_new(lru_cache_func_t* func);

/* 删除 LRU 缓存
 * 参数：cache - 待删除的缓存
 * 释放所有项和缓存结构
 */
void lru_cache_delete(lru_cache_t* cache);

/* 获取缓存项
 * 参数：cache - 缓存指针
 *       key - 键
 * 返回：找到返回值指针，未找到返回 NULL
 * 副作用：被访问的项移动到链表头部 (标记为最近使用)
 */
void* lru_cache_get(lru_cache_t* cache, void* key);

/* 放入缓存项
 * 参数：cache - 缓存指针
 *       key - 键
 *       val - 值
 * 返回：成功返回 0，失败返回 -1
 * 注意：如果容量已满，自动淘汰最久未使用的项
 */
int lru_cache_put(lru_cache_t* cache, void* key, void* val);

/* 移除缓存项
 * 参数：cache - 缓存指针
 *       key - 键
 * 从缓存中删除指定项
 */
void lru_cache_remove(lru_cache_t* cache, void* key);

/* 弹出最久未使用的项
 * 参数：cache - 缓存指针
 * 删除链表尾部的项 (最久未使用)
 */
void lru_cache_pop(lru_cache_t* cache);

/* 获取缓存迭代器
 * 参数：cache - 缓存指针
 * 返回：迭代器指针，用于遍历缓存项
 */
latte_iterator_t* lru_cache_get_iterator(lru_cache_t* cache);

/* 刷新缓存 (待实现)
 * 将缓存项持久化或同步到后端存储
 */

#endif /* __LATTE_LRU_CACHE_H */
