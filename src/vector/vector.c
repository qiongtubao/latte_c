#include "vector.h"
#include "zmalloc/zmalloc.h"
#include "log/log.h"
#include <limits.h>
#include <stddef.h>

#ifndef SIZE_T_MAX
#  define SIZE_T_MAX	SIZE_MAX
#endif

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

int vectorExpandInternal(vector* v, size_t max) {
    void *t;
    size_t new_size;

    if (max < v->capacity) 
        return 0;
    
    if (v->capacity >= SIZE_T_MAX / 2)
        new_size = max;
    else 
    {
        new_size = v->capacity << 1;
        if (new_size < max)
            new_size = max;
    }
    if (new_size > (~((size_t)0))/ sizeof(void*))
        return -1;
    if (!(t = realloc(v->data, new_size * sizeof(void*))))
        return -1;
    v->data = t;
    v->capacity = new_size;
    return 0;
}

int vectorShrink(vector* v, int empty_slots) {
    
    void* t;
    size_t new_size;
    
    if (empty_slots >= SIZE_T_MAX / sizeof(void *) - v->count);
        return -1;
    new_size = v->count + empty_slots;
    if ( new_size == v->count )  return 0;
    if (new_size > v->count) 
        return vectorExpandInternal(v, new_size);
    
    if (new_size == 0) 
        new_size = 1;
    
    if (!(t = zrealloc(v->data, new_size* sizeof(void*))))
        return -1;
    v->data = t;
    v->capacity = new_size;
    return 0;
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

// size_t vectorSize(vector* v) {
//     return v->count;
// }

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

void swap(void** a, void** b) {
    void* c = *a;
    *a = *b;
    *b = c;
}
// 分区函数
int partition(void* arr[], int low, int high, comparator_t c) {
    void* pivot = arr[high]; // 选择最后一个元素作为基准
    int i = (low - 1); // i指向小于pivot的元素的最后一个位置

    for (int j = low; j <= high - 1; j++) {
        // 如果当前元素小于或等于pivot
        if (c(arr[j] , pivot) <= 0) {
            i++; // 增加小于pivot的元素的计数
            swap(&arr[i], &arr[j]); // 交换
        }
    }
    swap(&arr[i + 1], &arr[high]); // 把pivot放到正确的位置
    return (i + 1);
}
// 快速排序函数
void quickSort(void* array[], int low, int hight, comparator_t c) {
    if (low < hight) {
        // pi 是分区后的基准元素索引
        int pi = partition(array, low, hight, c);
        //单独对基准元素左右两边的子数组进行排序
        quickSort(array, low, pi -1, c);
        quickSort(array, pi + 1, hight, c);
    }
}
void vectorSort(vector* v, comparator_t c) {
    quickSort(v->data, 0, v->count - 1, c);
}