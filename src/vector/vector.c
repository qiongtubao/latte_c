#include "vector.h"
#include "zmalloc/zmalloc.h"
#include "log/log.h"
#include <limits.h>
#include <stddef.h>

#ifndef SIZE_T_MAX
#  define SIZE_T_MAX	SIZE_MAX
#endif

vector_t* vector_new() {
    vector_t* v = zmalloc(sizeof(vector_t));
    if (v == NULL) {
        LATTE_LIB_LOG(LOG_ERROR, "malloc vector fail");
        return NULL;
    }
    v->data = NULL;
    v->capacity = 0;
    v->count = 0;
    return v;
}
void vector_delete(vector_t* v) {
    zfree(v->data);
    zfree(v);
}

int vector_expand_internal(vector_t* v, size_t max) {
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
    if (!(t = zrealloc(v->data, new_size * sizeof(void*))))
        return -1;
    v->data = t;
    v->capacity = new_size;
    return 0;
}

int vector_shrink(vector_t* v, int empty_slots) {
    
    void* t;
    size_t new_size;
    
    if ((unsigned long)empty_slots >= (SIZE_T_MAX / sizeof(void *) - v->count))
        return -1;
    new_size = v->count + empty_slots;
    if ( new_size == v->count )  return 0;
    if (new_size > v->count) 
        return vector_expand_internal(v, new_size);
    
    if (new_size == 0) 
        new_size = 1;
    
    if (!(t = zrealloc(v->data, new_size* sizeof(void*))))
        return -1;
    v->data = t;
    v->capacity = new_size;
    return 0;
}

void vector_resize(vector_t* v, size_t new_capacity) {
    void** new_data =zrealloc(v->data, new_capacity* sizeof(void*));
    if (new_data == NULL) {
        LATTE_LIB_LOG(LOG_ERROR, "zrealloc vector fail");
        exit(EXIT_FAILURE);
    }
    v->data = new_data;
    v->capacity = new_capacity;
}
int vector_push(vector_t* v, void* element) {
    if (v->count == v->capacity) {
        size_t new_capacity = v->capacity == 0 ? 1 : v->capacity * 2;
        vector_resize(v, new_capacity);
    }
    v->data[v->count++] = element;
    return 0;
}
void* vector_pop(vector_t* v) {
    if (v->count == 0) {
        return NULL;
    }
    void* element = v->data[v->count - 1];
    v->count--;
    return element;
}

// size_t vector_size(vector_t* v) {
//     return v->count;
// }

void* vector_get(const vector_t* v, size_t index) {
    if (index >= v->count) {
        return NULL;
    }
    return v->data[index];
}
void vector_set(vector_t* v, size_t index, void* element) {
    if (index >= v->count) {
        return;
    }
    v->data[index] = element;
}


//====== iterator ======
typedef struct vector_iterator_t {
    vector_t* v;
    size_t index;
} vector_iterator_t;

bool vector_iterator_has_next(latte_iterator_t* iterator) {
    vector_iterator_t* data = (vector_iterator_t*)(iterator->data);
    return data->index < data->v->count;
}

void* vector_iterator_next(latte_iterator_t* iterator) {
    vector_iterator_t* data = (vector_iterator_t*)(iterator->data);
    return vector_get(data->v, data->index++);
}

void vector_iterator_delete(latte_iterator_t* iterator) {
    zfree(iterator->data);
    zfree(iterator);
}

latte_iterator_func vector_iterator_type = {
    .has_next = vector_iterator_has_next,
    .next = vector_iterator_next,
    .release = vector_iterator_delete
};


/**
 * TODO 暂时没实现倒序遍历
 */
latte_iterator_t* vector_get_iterator(vector_t* v) {
    latte_iterator_t* iterator = zmalloc(sizeof(latte_iterator_t));
    vector_iterator_t* data = zmalloc(sizeof(vector_iterator_t));
    data->index = 0;
    data->v = v;
    iterator->data = data;
    iterator->func = &vector_iterator_type;
    return iterator;
}

void swap(void** a, void** b) {
    void* c = *a;
    *a = *b;
    *b = c;
}
// 分区函数
int partition(void* arr[], int low, int high, cmp_func c) {
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
void quick_sort(void* array[], int low, int hight, cmp_func c) {
    if (low < hight) {
        // pi 是分区后的基准元素索引
        int pi = partition(array, low, hight, c);
        //单独对基准元素左右两边的子数组进行排序
        quick_sort(array, low, pi -1, c);
        quick_sort(array, pi + 1, hight, c);
    }
}
void vector_sort(vector_t* v, cmp_func c) {
    quick_sort(v->data, 0, v->count - 1, c);
}

int private_vector_cmp(vector_t* a, vector_t* b, cmp_func cmp) {
    if (vector_size(a) != vector_size(b)) {
        return vector_size(a) - vector_size(b);
    }
    int cmp_result = 0;
    for(size_t i = 0; i < vector_size(a); i++) {
        if ((cmp_result = cmp(vector_get(a, i), vector_get(b, i))) != 0) {
            return cmp_result;
        }
    }
    return cmp_result;
}