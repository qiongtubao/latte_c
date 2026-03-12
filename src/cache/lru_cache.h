/**
 * @file lru_cache.h
 * @brief LRU（最近最少使用）缓存接口
 *        基于双向链表 + 哈希字典实现的 LRU 缓存：
 *        - 链表维护访问顺序（头部为最近访问，尾部为最久未访问）
 *        - 字典以 key 为索引存储链表节点指针，支持 O(1) 查找与删除
 *        适用于需要快速淘汰最久未使用元素的缓存场景。
 */
#ifndef __LATTE_LRU_CACHE_H
#define __LATTE_LRU_CACHE_H


#include "list/list.h"
#include "dict/dict.h"

//todo flush

/**
 * @brief LRU 缓存结构体
 *        dict 存储 list 节点的优势：数量多时 dict 查找比较快（删除节点块）
 */
typedef struct lru_cache_t {
    list_t* list;         /**< 有序链表，存储缓存值，头部为最近访问元素 */
    dict_t* searcher;    /**< 哈希字典，key → list_node_t*，用于 O(1) 查找节点 */
} lru_cache_t;

/**
 * @brief LRU 缓存函数表结构体
 *        继承自 dict_func_t，提供哈希、比较、析构等回调函数。
 */
typedef struct lru_cache_func_t {
    dict_func_t super; /**< 继承的字典函数表（哈希函数、键值析构等） */
} lru_cache_func_t;

/**
 * @brief 创建新的 LRU 缓存
 * @param func 缓存函数表指针，提供哈希/比较/析构等回调（不可为 NULL）
 * @return 新建的 lru_cache_t 指针；内存分配失败返回 NULL
 */
lru_cache_t* lru_cache_new(lru_cache_func_t* func);

/**
 * @brief 销毁 LRU 缓存
 *        释放缓存内部所有节点、链表及字典，但不释放节点中的 key/val 数据。
 * @param cache 要销毁的缓存指针（NULL 时安全跳过）
 */
void lru_cache_delete(lru_cache_t* cache);

/**
 * @brief 从缓存中查找指定 key 对应的值
 *        命中时将对应节点移至链表头部（标记为最近访问）。
 * @param cache 目标缓存指针
 * @param key   查询键指针
 * @return 找到返回对应值指针；未找到或 cache 为 NULL 返回 NULL
 */
void* lru_cache_get(lru_cache_t* cache, void* key);

/**
 * @brief 向缓存中插入或更新键值对
 *        若 key 已存在则更新值并移至头部；若不存在则在头部插入新节点。
 * @param cache 目标缓存指针
 * @param key   键指针
 * @param val   值指针
 * @return 成功返回 0（DICT_OK）；失败返回 1（DICT_ERR）
 */
int lru_cache_put(lru_cache_t* cache, void* key, void* val);

/**
 * @brief 从缓存中删除指定 key 对应的节点
 *        同时从链表和字典中移除，但不释放 key/val 数据本身。
 * @param cache 目标缓存指针
 * @param key   要删除的键指针
 */
void lru_cache_remove(lru_cache_t* cache, void* key);

/**
 * @brief 弹出（删除）链表尾部最久未使用的节点
 *        通常在缓存满时调用，用于淘汰最旧元素。
 * @param cache 目标缓存指针
 */
void lru_cache_pop(lru_cache_t* cache);

/**
 * @brief 获取 LRU 缓存的迭代器
 *        按链表顺序（从最近访问到最久未访问）遍历所有缓存值。
 * @param cache 目标缓存指针
 * @return 新建的 latte_iterator_t 指针；失败返回 NULL
 */
latte_iterator_t* lru_cache_get_iterator(lru_cache_t* cache);




/**  test  */

typedef struct latte_list {

} latte_list;
#endif
