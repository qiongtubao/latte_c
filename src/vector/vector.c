/**
 * @file vector.c
 * @brief 动态数组实现，提供可变长度数组功能和迭代器支持
 */
#include "vector.h"
#include "zmalloc/zmalloc.h"
#include "log/log.h"
#include <limits.h>
#include <stddef.h>

#ifndef SIZE_T_MAX
#  define SIZE_T_MAX	SIZE_MAX
#endif

/**
 * @brief 创建新的动态数组
 * @return 返回新创建的动态数组指针，失败返回NULL
 */
vector_t* vector_new() {
    vector_t* v = zmalloc(sizeof(vector_t));
    if (v == NULL) {
        LATTE_LIB_LOG(LOG_ERROR, "malloc vector fail");
        return NULL;
    }
    v->data = NULL;       /**< 初始化数据指针为空 */
    v->capacity = 0;      /**< 初始化容量为0 */
    v->count = 0;         /**< 初始化元素数量为0 */
    return v;
}

/**
 * @brief 删除动态数组，释放所有内存
 * @param v 要删除的动态数组指针
 */
void vector_delete(vector_t* v) {
    zfree(v->data);
    zfree(v);
}

/**
 * @brief 内部扩展数组容量函数
 * @param v 动态数组指针
 * @param max 最小所需容量
 * @return 成功返回0，失败返回-1
 */
int vector_expand_internal(vector_t* v, size_t max) {
    void *t;
    size_t new_size;

    if (max < v->capacity)
        return 0;

    // 防止容量溢出，采用渐进式扩容策略
    if (v->capacity >= SIZE_T_MAX / 2)
        new_size = max;
    else
    {
        new_size = v->capacity << 1; // 容量翻倍
        if (new_size < max)
            new_size = max;
    }

    // 检查是否会导致内存溢出
    if (new_size > (~((size_t)0))/ sizeof(void*))
        return -1;

    if (!(t = zrealloc(v->data, new_size * sizeof(void*))))
        return -1;

    v->data = t;
    v->capacity = new_size;
    return 0;
}

/**
 * @brief 收缩数组容量
 * @param v 动态数组指针
 * @param empty_slots 保留的空闲槽位数
 * @return 成功返回0，失败返回-1
 */
int vector_shrink(vector_t* v, int empty_slots) {

    void* t;
    size_t new_size;

    // 检查参数是否会导致溢出
    if ((unsigned long)empty_slots >= (SIZE_T_MAX / sizeof(void *) - v->count))
        return -1;

    new_size = v->count + empty_slots;
    if ( new_size == v->count )  return 0;

    // 如果新大小大于当前元素数，调用扩展函数
    if (new_size > v->count)
        return vector_expand_internal(v, new_size);

    // 确保至少保留1个槽位
    if (new_size == 0)
        new_size = 1;

    if (!(t = zrealloc(v->data, new_size* sizeof(void*))))
        return -1;

    v->data = t;
    v->capacity = new_size;
    return 0;
}

/**
 * @brief 调整数组容量到指定大小
 * @param v 动态数组指针
 * @param new_capacity 新的容量大小
 */
void vector_resize(vector_t* v, size_t new_capacity) {
    void** new_data =zrealloc(v->data, new_capacity* sizeof(void*));
    if (new_data == NULL) {
        LATTE_LIB_LOG(LOG_ERROR, "zrealloc vector fail");
        exit(EXIT_FAILURE);
    }
    v->data = new_data;
    v->capacity = new_capacity;
}

/**
 * @brief 向数组末尾添加元素
 * @param v 动态数组指针
 * @param element 要添加的元素指针
 * @return 成功返回0
 */
int vector_push(vector_t* v, void* element) {
    // 如果容量不足，自动扩容
    if (v->count == v->capacity) {
        size_t new_capacity = v->capacity == 0 ? 1 : v->capacity * 2;
        vector_resize(v, new_capacity);
    }
    v->data[v->count++] = element;
    return 0;
}

/**
 * @brief 从数组末尾移除并返回元素
 * @param v 动态数组指针
 * @return 返回移除的元素指针，数组为空时返回NULL
 */
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

/**
 * @brief 获取指定索引位置的元素
 * @param v 动态数组指针
 * @param index 元素索引
 * @return 返回元素指针，索引越界时返回NULL
 */
void* vector_get(const vector_t* v, size_t index) {
    if (index >= v->count) {
        return NULL;
    }
    return v->data[index];
}

/**
 * @brief 设置指定索引位置的元素
 * @param v 动态数组指针
 * @param index 元素索引
 * @param element 新的元素指针
 */
void vector_set(vector_t* v, size_t index, void* element) {
    if (index >= v->count) {
        return;
    }
    v->data[index] = element;
}


//====== iterator ======
/**
 * @brief 动态数组迭代器结构体
 */
typedef struct vector_iterator_t {
    vector_t* v;     /**< 被迭代的数组指针 */
    size_t index;    /**< 当前迭代位置索引 */
} vector_iterator_t;

/**
 * @brief 检查迭代器是否还有下一个元素
 * @param iterator 迭代器指针
 * @return 有下一个元素返回true，否则返回false
 */
bool vector_iterator_has_next(latte_iterator_t* iterator) {
    vector_iterator_t* data = (vector_iterator_t*)(iterator->data);
    return data->index < data->v->count;
}

/**
 * @brief 获取迭代器的下一个元素
 * @param iterator 迭代器指针
 * @return 返回下一个元素指针
 */
void* vector_iterator_next(latte_iterator_t* iterator) {
    vector_iterator_t* data = (vector_iterator_t*)(iterator->data);
    return vector_get(data->v, data->index++);
}

/**
 * @brief 删除动态数组迭代器
 * @param iterator 要删除的迭代器指针
 */
void vector_iterator_delete(latte_iterator_t* iterator) {
    zfree(iterator->data);
    zfree(iterator);
}

/**< 动态数组迭代器函数表 */
latte_iterator_func vector_iterator_type = {
    .has_next = vector_iterator_has_next,
    .next = vector_iterator_next,
    .release = vector_iterator_delete
};


/**
 * @brief 为动态数组创建迭代器
 * @param v 动态数组指针
 * @return 返回新创建的迭代器指针
 * @note TODO 暂时没实现倒序遍历
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

/**
 * @brief 交换两个指针的值
 * @param a 第一个指针的地址
 * @param b 第二个指针的地址
 */
void swap(void** a, void** b) {
    void* c = *a;
    *a = *b;
    *b = c;
}

/**
 * @brief 快速排序分区函数
 * @param arr 待排序数组
 * @param low 起始索引
 * @param high 结束索引
 * @param c 比较函数指针
 * @return 返回分区点索引
 */
int partition(void* arr[], int low, int high, cmp_func c) {
    void* pivot = arr[high]; // 选择最后一个元素作为基准
    int i = (low - 1); // i指向小于pivot的元素的最后一个位置

    for (int j = low; j <= high - 1; j++) {
        // 如果当前元素小于或等于pivot
        if (c(arr[j] , pivot) <= 0) {
            i++; // 增加小于pivot的元素的计数
            swap(&arr[i], &arr[j]); // 交换元素
        }
    }
    swap(&arr[i + 1], &arr[high]); // 把pivot放到正确的位置
    return (i + 1);
}

/**
 * @brief 快速排序递归实现
 * @param array 待排序数组
 * @param low 起始索引
 * @param hight 结束索引
 * @param c 比较函数指针
 */
void quick_sort(void* array[], int low, int hight, cmp_func c) {
    if (low < hight) {
        // pi 是分区后的基准元素索引
        int pi = partition(array, low, hight, c);
        //单独对基准元素左右两边的子数组进行排序
        quick_sort(array, low, pi -1, c);
        quick_sort(array, pi + 1, hight, c);
    }
}

/**
 * @brief 对动态数组进行排序
 * @param v 动态数组指针
 * @param c 比较函数指针
 */
void vector_sort(vector_t* v, cmp_func c) {
    quick_sort(v->data, 0, v->count - 1, c);
}

/**
 * @brief 比较两个动态数组
 * @param a 第一个数组指针
 * @param b 第二个数组指针
 * @param cmp 元素比较函数指针
 * @return 比较结果：<0表示a<b，=0表示a=b，>0表示a>b
 */
int private_vector_cmp(vector_t* a, vector_t* b, cmp_func cmp) {
    // 首先比较数组大小
    if (vector_size(a) != vector_size(b)) {
        return vector_size(a) - vector_size(b);
    }

    int cmp_result = 0;
    // 逐个比较数组元素
    for(size_t i = 0; i < vector_size(a); i++) {
        if ((cmp_result = cmp(vector_get(a, i), vector_get(b, i))) != 0) {
            return cmp_result;
        }
    }
    return cmp_result;
}

/**
 * @brief 从数组中移除指定元素（将其设置为NULL）
 * @param v 动态数组指针
 * @param element 要移除的元素指针
 */
void vector_remove(vector_t* v, void* element) {
    for (size_t i = 0; i < v->count; i++) {
        if (v->data[i] == element) {
            v->data[i] = NULL;
        }
    }
}

/**
 * @brief 添加元素到数组中（优先填充NULL位置）
 * @param v 动态数组指针
 * @param element 要添加的元素指针
 * @return 返回元素在数组中的索引位置
 */
int vector_add(vector_t* v, void* element) {
    // 首先尝试找到空的位置（NULL）来填充
    for (size_t i = 0; i < v->count; i++) {
        if (v->data[i] == NULL) {
            v->data[i] = element;
            return i;
        }
    }
    // 如果没有找到空位置，则在末尾添加
    vector_push(v, element);
    return v->count - 1;
}