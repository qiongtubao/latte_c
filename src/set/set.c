#include "set.h"

set* setCreate(setType* type, void* data) {
    set* s = zmalloc(sizeof(set));
    s->data = data;
    s->type = type;
    return s;
}
// -1 失败  0 已经存在  1 新添加
int setAdd(set* s, void* key) {
    return s->type->add(s, key);
}
//0 不存在 1 存在
int setContains(set* s, void* key) {
    return s->type->contains(s, key);
}

int setRemove(set* s, void* key) {
    return s->type->remove(s, key);
}

size_t setSize(set* s) {
    return s->type->size(s);
}

void setRelease(set* s) {
    s->type->release(s);
}

Iterator* setGetIterator(set* s) {
    return s->type->getIterator(s);
}
