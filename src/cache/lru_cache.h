#ifndef __LATTE_LRU_CACHE_H
#define __LATTE_LRU_CACHE_H


#include "list/list.h"
#include "dict/dict.h"

//todo flush 

/**
 *   dict 存储list节点优势是在与 数量多的时 dict查找比较块  （删除node节点块）
 */
typedef struct lru_cache_t { 
    list_t* list;         // list<value>
    dict_t* searcher;    // map<key, list_node_t>
} lru_cache_t;

typedef struct lru_cache_func_t {
    dict_func_t super;
} lru_cache_func_t;

lru_cache_t* lru_cache_new(lru_cache_func_t* func);
void* lru_cache_get(lru_cache_t* cache, void* key);
int lru_cache_put(lru_cache_t* cache, void* key, void* val);
void lru_cache_remove(lru_cache_t* cache, void* key);


latte_iterator_t* lru_cache_get_iterator(lru_cache_t* cache);




/**  test  */

typedef struct latte_list {

} latte_list;
#endif