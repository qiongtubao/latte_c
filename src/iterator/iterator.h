#ifndef __LATTE_ITERATOR_H
#define __LATTE_ITERATOR_H

#include <stdlib.h>
#include <stdbool.h>
typedef struct Iterator Iterator;
typedef struct IteratorType {
    bool (*hasNext)(Iterator* iterator);
    void* (*next)(Iterator* iterator);
    void (*release)(Iterator* iterator);
} IteratorType;

typedef struct Iterator {
    IteratorType* type;
    void* data;
} Iterator;

bool iteratorHasNext(Iterator* iterator) ;

 void* iteratorNext(Iterator* iterator) ;

 void iteratorRelease(Iterator* iterator);




#endif