#include "vector.h"
#include "zmalloc/zmalloc.h"
#include "log/log.h"
vector* vectorCreate() {
    vector* v = zmalloc(sizeof(vector));
    if (v == NULL) {
        LATTE_LIB_LOG(LOG_ERROR, "malloc vector fail");
        return NULL;
    }
    v->data = NULL;
    v->capacity = 0;
    v->count = 0;
    return v;
}
void vectorRelease(vector* v) {
    zfree(v->data);
    zfree(v);
}
void vectorResize(vector* v, size_t new_capacity) {
    void** new_data =zrealloc(v->data, new_capacity* sizeof(void*));
    if (new_data == NULL) {
        LATTE_LIB_LOG(LOG_ERROR, "zrealloc vector fail");
        exit(EXIT_FAILURE);
    }
    v->data = new_data;
    v->capacity = new_capacity;
}
int vectorPush(vector* v, void* element) {
    if (v->count == v->capacity) {
        size_t new_capacity = v->capacity == 0 ? 1 : v->capacity * 2;
        vectorResize(v, new_capacity);
    }
    v->data[v->count++] = element;
    return 0;
}
void* vectorPop(vector* v) {
    if (v->count == 0) {
        return NULL;
    }
    void* element = v->data[v->count - 1];
    v->count--;
    return element;
}
size_t vectorSize(vector* v) {
    return v->count;
}
void* vectorGet(const vector* v, size_t index) {
    if (index >= v->count) {
        return NULL;
    }
    return v->data[index];
}
void vectorSet(vector* v, size_t index, void* element) {
    if (index >= v->count) {
        return;
    }
    v->data[index] = element;
}


//====== iterator ======
typedef struct vectorIteratorData {
    vector* v;
    size_t index;
} vectorIteratorData;
bool vectorIteratorhasNext(Iterator* iterator) {
    vectorIteratorData* data = (vectorIteratorData*)(iterator->data);
    return data->index < data->v->count;
}

void* vectorIteratorNext(Iterator* iterator) {
    vectorIteratorData* data = (vectorIteratorData*)(iterator->data);
    return vectorGet(data->v, data->index++);
}

void vectorIteratorRelease(Iterator* iterator) {
    zfree(iterator->data);
    zfree(iterator);
}
IteratorType vector_iterator_type = {
    .hasNext = vectorIteratorhasNext,
    .next = vectorIteratorNext,
    .release = vectorIteratorRelease
};


/**
 * TODO 暂时没实现倒序遍历
 */
Iterator* vectorGetIterator(vector* v) {
    Iterator* iterator = zmalloc(sizeof(Iterator));
    vectorIteratorData* data = zmalloc(sizeof(vectorIteratorData));
    data->index = 0;
    data->v = v;
    iterator->data = data;
    iterator->type = &vector_iterator_type;
    return iterator;
}