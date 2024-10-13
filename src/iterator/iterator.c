#include "iterator.h"
bool latte_iterator_has_next(latte_iterator_t* iterator) {
    return iterator->type->hasNext(iterator);
}

void* latte_iterator_next(latte_iterator_t* iterator) {
    return iterator->type->next(iterator);
}

void latte_iterator_delete(latte_iterator_t* iterator) {
    return iterator->type->release(iterator);
}