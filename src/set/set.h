


#ifndef __LATTE_SET_H
#define __LATTE_SET_H

#include <dict/dict.h>
#include <dict_plugins/dict_plugins.h>
#include <stdbool.h>
#include "iterator/iterator.h"
#include "zmalloc/zmalloc.h"



typedef struct set set;

// typedef struct setNode setNode;
typedef struct setType {
    // char* (*getName)();
    int (*add)(set* set, void* element);
    int (*remove)(set* set, void* element);
    int (*contains)(set* set, void* element);
    size_t (*size)(set* set);
    void (*release)(set* set);
    Iterator* (*getIterator)(set* set);
} setType;
//主要是用于不同set类型 如果无法明确用途就使用动态方式，
//如果确定的话最好就用指定类型
typedef struct set {
    setType* type;
    void* data;
} set;

// typedef struct setIteratorType {
//     bool  (*hasNext)(setIterator* iterator);
//     void* (*next)(setIterator* iterator);
//     void (*release)(setIterator* iterator);
// } setIteratorType;
// typedef struct setIterator {
//     setIteratorType* type;
//     void* data;
// } setIterator;

typedef struct setKeyType {

} setKeyType;

set* setCreate(setType* type, void* data);
// -1 失败  0 已经存在  1 新添加
int setAdd(set* s, void* key);
//0 不存在 1 存在
int setContains(set* s, void* key);

int setRemove(set* s, void* key);

size_t setSize(set* s);

void setRelease(set* s);

Iterator* setGetIterator(set* s);

// in avlSet.h
// set* setCreateAvl(avlTreeType* type); 
// in hashSet.h
//set* setCreateHash(hashSetType* type);
#endif