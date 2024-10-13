#ifndef __LATTE_ITERATOR_H
#define __LATTE_ITERATOR_H

#include <stdlib.h>
#include <stdbool.h>
typedef struct latte_iterator_t latte_iterator_t;

typedef struct latte_iterator_func {
    bool (*hasNext)(latte_iterator_t* iterator);
    void* (*next)(latte_iterator_t* iterator);
    void (*release)(latte_iterator_t* iterator);
} latte_iterator_func;

typedef struct latte_iterator_t {
    latte_iterator_func* type;
    void* data;
} latte_iterator_t;

// bool latte_iterator_has_next(latte_iterator_t* iterator) ;
bool  latte_iterator_has_next(latte_iterator_t* iterator);
void* latte_iterator_next(latte_iterator_t* iterator);
void  latte_iterator_delete(latte_iterator_t* iterator);


// 一些通用类和参数

typedef struct latte_pair_t {
    void* key;
    void* value;
} latte_pair_t;
#define latte_pair_get_key(pair) pair->key
#define latte_pair_get_value(pair) pair->value


#endif