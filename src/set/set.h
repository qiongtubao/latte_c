


#ifndef __LATTE_SET_H
#define __LATTE_SET_H

#include "dict/dict.h"
#include "dict/dict_plugins.h"
#include <stdbool.h>
#include "iterator/iterator.h"
#include "zmalloc/zmalloc.h"



typedef struct set_t set_t;

// typedef struct setNode setNode;
typedef struct set_func_t {
    // char* (*getName)();
    int (*add)(set_t* set, void* element);
    int (*remove)(set_t* set, void* element);
    int (*contains)(set_t* set, void* element);
    size_t (*size)(set_t* set);
    void (*release)(set_t* set);
    void (*clear)(set_t* set);
    latte_iterator_t* (*getIterator)(set_t* set);
} set_func_t;
//主要是用于不同set类型 如果无法明确用途就使用动态方式，
//如果确定的话最好就用指定类型
typedef struct set_t {
    set_func_t* type;
    void* data;
} set_t;

// typedef struct setIteratorType {
//     bool  (*hasNext)(setIterator* iterator);
//     void* (*next)(setIterator* iterator);
//     void (*release)(setIterator* iterator);
// } setIteratorType;
// typedef struct setIterator {
//     setIteratorType* type;
//     void* data;
// } setIterator;

// typedef struct setKeyType {

// } setKeyType;

set_t* set_new(set_func_t* type, void* data);
// -1 失败  0 已经存在  1 新添加
int set_add(set_t* s, void* key);
//0 不存在 1 存在
int set_contains(set_t* s, void* key);

int set_remove(set_t* s, void* key);

size_t set_size(set_t* s);

void set_delete(set_t* s);
void set_clear(set_t* s);

latte_iterator_t* set_get_iterator(set_t* s);

// in avlSet.h
// set_t* avl_set_new(avl_tree_type_t* type); 
// in hash_set.h
//set_t* hash_set_new(hash_set_func_t* type);
#endif