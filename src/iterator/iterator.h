#ifndef __LATTE_ITERATOR_H
#define __LATTE_ITERATOR_H

#include <stdlib.h>
#include <stdbool.h>
typedef struct latte_iterator_t latte_iterator_t;


/**
 *  TODO
 *  暂时未增加reset函数如何实现
 */ 
typedef struct latte_iterator_func {
    bool (*has_next)(latte_iterator_t* iterator);
    void* (*next)(latte_iterator_t* iterator);
    void (*release)(latte_iterator_t* iterator);
} latte_iterator_func;

// 一些通用类和参数
typedef struct latte_pair_t {
    void* key;
    void* value;
} latte_pair_t;
#define latte_pair_key(pair) ((latte_pair_t*)pair)->key
#define latte_pair_value(pair) ((latte_pair_t*)pair)->value
typedef struct latte_iterator_t {
    latte_iterator_func* func;
    void* data;
} latte_iterator_t;

typedef struct latte_iterator_pair_t {
    latte_iterator_t sup;
    latte_pair_t pair;
} latte_iterator_pair_t;

#define iterator_pair_key(it)  ((latte_iterator_pair_t*)it)->pair.key
#define iterator_pair_value(it)  ((latte_iterator_pair_t*)it)->pair.value
#define iterator_pair_ptr(it)   &(((latte_iterator_pair_t*)it)->pair)

latte_iterator_t* latte_iterator_new(latte_iterator_func* func, void* data);
void latte_iterator_init(latte_iterator_t* it, latte_iterator_func* func, void* data);
latte_iterator_t* latte_iterator_pair_new(latte_iterator_func* func, void* data);
void latte_iterator_pair_init(latte_iterator_pair_t* it, latte_iterator_func* func, void* data);


bool  latte_iterator_has_next(latte_iterator_t* iterator);
void* latte_iterator_next(latte_iterator_t* iterator);
void  latte_iterator_delete(latte_iterator_t* iterator);







#endif