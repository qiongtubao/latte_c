/**
 * @file lru_cache.c
 * @brief LRU（最近最少使用）缓存实现
 *        结合链表和哈希表实现高效的LRU缓存机制
 *        支持O(1)时间复杂度的插入、查找和删除操作
 */

#include "lru_cache.h"
#include "zmalloc/zmalloc.h"
#include <assert.h>

/**
 * @brief 创建新的LRU缓存
 *        初始化双向链表和哈希表用于快速访问
 * @param func 缓存函数表，包含键值比较和哈希函数
 * @return 新创建的LRU缓存；失败返回NULL
 */
lru_cache_t* lru_cache_new(lru_cache_func_t* func) {
    lru_cache_t* cache = zmalloc(sizeof(lru_cache_t));
    cache->list = list_new();                    // 创建双向链表存储缓存项
    cache->searcher = dict_new(&func->super);    // 创建哈希表用于快速查找
    return cache;
}

/**
 * @brief 删除LRU缓存
 *        释放所有相关资源包括链表和哈希表
 * @param cache 要删除的缓存实例
 */
void lru_cache_delete(lru_cache_t* cache) {
    list_delete(cache->list);      // 删除链表
    dict_delete(cache->searcher);  // 删除哈希表
    zfree(cache);
}

/**
 * @brief 获取缓存中的元素数量
 * @param cache 缓存实例
 * @return 缓存中的元素数量
 */
size_t lru_cache_len(lru_cache_t* cache) {
    return dict_size(cache->searcher);
}

/**
 * @brief 从缓存中获取值
 *        如果键存在，将对应节点移到链表头部（标记为最近使用）
 * @param cache 缓存实例
 * @param key   查找的键
 * @return 对应的值；未找到返回NULL
 */
void* lru_cache_get(lru_cache_t* cache, void* key) {
    dict_entry_t* entry = dict_find(cache->searcher, key);
    if (entry == NULL) {
        return NULL;  // 键不存在
    }
    // 获取对应的链表节点
    list_node_t* node = (list_node_t*)dict_get_entry_val((entry));
    // 将节点移到链表头部，标记为最近使用
    list_move_head(cache->list, node);
    return list_node_value(node);  // 返回节点中存储的值
}

/**
 * @brief 向缓存中插入或更新键值对
 *        如果键已存在则更新值，否则插入新键值对到链表头部
 * @param cache 缓存实例
 * @param key   键
 * @param val   值
 * @return 1 插入新键值对；0 更新已存在的键值对；-1 失败
 */
int lru_cache_put(lru_cache_t* cache, void* key, void* val) {
    dict_entry_t* entry = dict_find(cache->searcher, key);
    if (entry != NULL) {
        // 键已存在，更新值并移到头部
        list_node_t* node = (list_node_t*)dict_get_entry_val(entry);
        latte_pair_value(list_node_value(node)) = val;  // 更新值
        list_move_head(cache->list, node);              // 移到头部
        return 0;
    }
    // 创建新的键值对
    latte_pair_t* pair = latte_pair_new(key, val);
    if (pair == NULL) return -1;

    // 将新键值对添加到链表头部
    cache->list = list_add_node_head(cache->list, pair);
    assert(cache->list->head != NULL);

    // 在哈希表中建立键到链表节点的映射
    return DICT_OK == dict_add(cache->searcher, key, cache->list->head) ? 1: -1;
}

/**
 * @brief 从缓存中移除指定键的条目
 * @param cache 缓存实例
 * @param key   要移除的键
 */
void lru_cache_remove(lru_cache_t* cache, void* key) {
    dict_entry_t* node = dict_find(cache->searcher, key);
    if (node != NULL) {
        // 从链表中删除节点
        list_del_node(cache->list, dict_get_entry_val(node));
        // 从哈希表中删除键
        dict_delete_key(cache->searcher, key);
    }
}

/**
 * @brief 移除最近最少使用的条目（链表尾部元素）
 *        用于缓存满时腾出空间
 * @param cache 缓存实例
 */
void lru_cache_pop(lru_cache_t* cache) {
    // 获取链表尾部的键值对（最少使用的）
    latte_pair_t*  pair = (latte_pair_t*)list_node_value(cache->list->tail);
    // 从哈希表中删除对应的键
    assert(DICT_OK == dict_delete_key(cache->searcher, latte_pair_key(pair)));
    // 从链表中删除尾部节点
    list_del_node(cache->list, cache->list->tail);
}

// void* lru_cache_iterator_next(latte_iterator_t* iter) {
//     latte_pair_t* pair = protected_latte_list_iterator_next(iter);
//     pair->value = list_node_value((list_node_t*)pair->value);
//     return pair;
// }

// latte_iterator_func lru_cache_iterator_func = {
//     .has_next = protected_latte_list_iterator_has_next,
//     .next = lru_cache_iterator_next,
//     .release = protected_latte_list_iterator_delete
// };

/**
 * @brief 获取LRU缓存的迭代器
 *        用于遍历缓存中的所有元素（从最近使用到最少使用）
 * @param cache 缓存实例
 * @return 链表迭代器，从头部开始遍历
 */
latte_iterator_t* lru_cache_get_iterator(lru_cache_t* cache) {
    latte_iterator_t* it = list_get_latte_iterator(cache->list, LIST_ITERATOR_OPTION_HEAD);
    return it;
}