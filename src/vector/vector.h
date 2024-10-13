#ifndef __LATTE_VECTOR_H
#define __LATTE_VECTOR_H
#include <stdlib.h>
#include <stdio.h>
#include "iterator/iterator.h"
/**
 *  非线程安全的
 */
typedef struct vector_t {
    void **data;
    size_t capacity;
    size_t count;
} vector_t;
vector_t* vector_new();
void vector_delete(vector_t* v);
void vector_resize(vector_t* v, size_t new_capacity);
int vector_push(vector_t* v, void* element);
void* vector_pop(vector_t* v);
// size_t vector_size(vector_t* v);
#define vector_size(v) v->count

void* vector_get(const vector_t* v, size_t index);
void vector_set(vector_t* v, size_t index, void* element);
int vector_shrink(vector_t* v, int empty_slots);
typedef int comparator_func(void* v1, void* v2);
void vector_sort(vector_t* v, comparator_func c);

#define vector_get_last(v) vector_get(v, v->count - 1)
#define vector_get_frist(v) vector_get(v , 0)

latte_iterator_t* vector_get_iterator(vector_t* v);


#endif
