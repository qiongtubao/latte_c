#include "iterator.h"
bool iteratorHasNext(Iterator* iterator) {
    return iterator->type->hasNext(iterator);
}

void* iteratorNext(Iterator* iterator) {
    return iterator->type->next(iterator);
}

void iteratorRelease(Iterator* iterator) {
    return iterator->type->release(iterator);
}