#include "iterator.h"
#include "zmalloc/zmalloc.h"
bool latte_iterator_has_next(latte_iterator_t* iterator) {
    return iterator->func->has_next(iterator);
}

void* latte_iterator_next(latte_iterator_t* iterator) {
    return iterator->func->next(iterator);
}

void latte_iterator_delete(latte_iterator_t* iterator) {
    return iterator->func->release(iterator);
}



void latte_iterator_init(latte_iterator_t* it, latte_iterator_func* func, void* data) {
    it->func = func;
    it->data = data;
}

latte_iterator_t* latte_iterator_new(latte_iterator_func* func, void* data) {
    latte_iterator_t* it = zmalloc(sizeof(latte_iterator_t));
    latte_iterator_init(it, func, data);
    return it;
}

void latte_iterator_pair_init(latte_iterator_pair_t* it, latte_iterator_func* func, void* data) {
    latte_iterator_init(&it->sup, func, data);
    it->pair.key = NULL;
    it->pair.value = NULL;
}

latte_iterator_t* latte_iterator_pair_new(latte_iterator_func* func, void* data) {
    latte_iterator_pair_t* it = zmalloc(sizeof(latte_iterator_pair_t));
    latte_iterator_pair_init(it, func, data);
    return (latte_iterator_t*)it;
}