#include "set.h"

set_t* set_new(set_func_t* type, void* data) {
    set_t* s = zmalloc(sizeof(set_t));
    s->data = data;
    s->type = type;
    return s;
}
// -1 失败  0 已经存在  1 新添加
int set_add(set_t* s, void* key) {
    return s->type->add(s, key);
}
//0 不存在 1 存在
int set_contains(set_t* s, void* key) {
    return s->type->contains(s, key);
}

int set_remove(set_t* s, void* key) {
    return s->type->remove(s, key);
}

size_t set_size(set_t* s) {
    return s->type->size(s);
}

void set_delete(set_t* s) {
    s->type->release(s);
}

latte_iterator_t* set_get_iterator(set_t* s) {
    return s->type->getIterator(s);
}
