#include "lru_cache.h"
#include "zmalloc/zmalloc.h"
#include <assert.h>

lru_cache_t* lru_cache_new(lru_cache_func_t* func) {
    lru_cache_t* cache = zmalloc(sizeof(lru_cache_t));
    cache->list = list_new();
    cache->searcher = dict_new(func);
    return cache;
}

size_t lru_cache_len(lru_cache_t* cache) {
    return dict_size(cache->searcher);
}

void* lru_cache_get(lru_cache_t* cache, void* key) {
    dict_entry_t* entry = dict_find(cache->searcher, key);
    if (entry == NULL) {
        return NULL;
    }
    list_node_t* node = (list_node_t*)dict_get_entry_val((entry));
    list_move_head(cache->list, node);
    return list_node_value(node);
}

int lru_cache_put(lru_cache_t* cache, void* key, void* val) {
    dict_entry_t* entry = dict_find(cache->searcher, key);
    list_node_t* node = NULL;
    if (entry != NULL) {
        node = (list_node_t*)dict_get_entry_val(entry);
        list_node_value(node) = val;
        list_move_head(cache->list, node);
        return 0;
    }
    cache->list = list_add_node_head(cache->list, val);
    assert(cache->list->head != NULL);
    return DICT_OK == dict_add(cache->searcher, key, cache->list->head) ? 1: -1;
}

void lru_cache_remove(lru_cache_t* cache, void* key) {
    dict_entry_t* node = dict_find(cache->searcher, key);
    if (node != NULL) {
        list_del_node(cache->list, dict_get_entry_val(node));
        dict_delete_key(cache->searcher, key);
    }
}

void* lru_cache_iterator_next(latte_iterator_t* iter) {
    latte_pair_t* pair = protected_dict_iterator_next(iter);
    pair->value = list_node_value((list_node_t*)pair->value);
    return pair;
}

latte_iterator_func lru_cache_iterator_func = {
    .has_next = protected_dict_iterator_has_next,
    .next = lru_cache_iterator_next,
    .release = protected_dict_iterator_delete
};

latte_iterator_t* lru_cache_get_iterator(lru_cache_t* cache) {
    latte_iterator_t* it =  dict_get_latte_iterator(cache->searcher);
    it->func = &lru_cache_iterator_func;
    return it;
}



