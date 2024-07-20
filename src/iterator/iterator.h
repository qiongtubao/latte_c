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

inline  bool iteratorHasNext(Iterator* iterator) {
    return iterator->type->hasNext(iterator);
}

inline void* iteratorNext(Iterator* iterator) {
    return iterator->type->next(iterator);
}

inline void iteratorRelease(Iterator* iterator) {
    return iterator->type->release(iterator);
}




#endif