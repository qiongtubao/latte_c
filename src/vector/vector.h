#ifndef __LATTE_VECTOR_H
#define __LATTE_VECTOR_H
#include <stdlib.h>
#include <stdio.h>
#include "iterator/iterator.h"
/**
 *  非线程安全的
 */
typedef struct vector {
    void **data;
    size_t capacity;
    size_t count;
} vector;
vector* vectorCreate();
void vectorRelease(vector* v);
void vectorResize(vector* v, size_t new_capacity);
int vectorPush(vector* v, void* element);
void* vectorPop(vector* v);
// size_t vectorSize(vector* v);
#define vectorSize(v) v->count

void* vectorGet(const vector* v, size_t index);
void vectorSet(vector* v, size_t index, void* element);
int vectorShrink(vector* v, int empty_slots);
typedef int comparator_t(void* v1, void* v2);
void vectorSort(vector* v, comparator_t c);


Iterator* vectorGetIterator(vector* v);


#endif
