#ifndef __LATTE_VECTOR_H
#define __LATTE_VECTOR_H
#include <stdlib.h>
#include <stdio.h>
#include "cmp/cmp.h"
#include "iterator/iterator.h"
#include "cmp/cmp.h"

/**
 * @brief 动态数组（非线程安全）
 *
 * 基于 void* 指针数组实现的泛型动态数组，支持自动扩容。
 * 不保证线程安全，并发访问需自行加锁。
 */
typedef struct vector_t {
    void **data;        /**< 元素指针数组（堆分配） */
    size_t capacity;    /**< 当前已分配的容量（元素个数） */
    size_t count;       /**< 当前存储的元素数量 */
} vector_t;

/**
 * @brief 创建一个空动态数组
 * @return vector_t* 新建数组指针
 */
vector_t* vector_new();

/**
 * @brief 释放动态数组及其内部 data 数组（不释放元素本身）
 * @param v 目标数组指针
 */
void vector_delete(vector_t* v);

/**
 * @brief 将数组容量调整为 new_capacity（扩容或缩容）
 * @param v            目标数组指针
 * @param new_capacity 新容量（元素个数）
 */
void vector_resize(vector_t* v, size_t new_capacity);

/**
 * @brief 向数组尾部追加元素（可能触发自动扩容）
 * @param v       目标数组指针
 * @param element 要追加的元素指针
 * @return int 成功返回 0
 */
int vector_push(vector_t* v, void* element);

/**
 * @brief 弹出并返回数组尾部元素（不缩容）
 * @param v 目标数组指针
 * @return void* 弹出的元素指针，数组为空时行为未定义
 */
void* vector_pop(vector_t* v);

/**
 * @brief 从数组中移除第一个与 element 相等的元素（指针比较）
 * @param v       目标数组指针
 * @param element 要移除的元素指针
 */
void vector_remove(vector_t* v, void* element);

/**
 * @brief 向数组末尾添加元素（同 vector_push）
 * @param v       目标数组指针
 * @param element 要添加的元素指针
 * @return int 成功返回 0
 */
int vector_add(vector_t* v, void* element);

/** @brief 获取数组当前元素数量（宏，直接访问 count 字段） */
#define vector_size(v) v->count

/**
 * @brief 获取指定下标的元素（不检查越界）
 * @param v     目标数组指针
 * @param index 元素下标（0-based）
 * @return void* 元素指针
 */
void* vector_get(const vector_t* v, size_t index);

/**
 * @brief 设置指定下标的元素（不检查越界）
 * @param v       目标数组指针
 * @param index   元素下标
 * @param element 新元素指针
 */
void vector_set(vector_t* v, size_t index, void* element);

/**
 * @brief 尝试缩减数组容量，保留 empty_slots 个空余槽位
 * @param v           目标数组指针
 * @param empty_slots 缩容后保留的空余槽位数
 * @return int 成功返回 0
 */
int vector_shrink(vector_t* v, int empty_slots);

/**
 * @brief 对数组元素进行排序
 * @param v 目标数组指针
 * @param c 比较函数（cmp_func 类型）
 */
void vector_sort(vector_t* v, cmp_func c);

/** @brief 获取最后一个元素（宏） */
#define vector_get_last(v)  vector_get(v, v->count - 1)
/** @brief 获取第一个元素（宏） */
#define vector_get_frist(v) vector_get(v, 0)

/**
 * @brief 获取数组的迭代器（支持 latte_iterator_t 接口）
 * @param v 目标数组指针
 * @return latte_iterator_t* 迭代器指针（调用方负责释放）
 */
latte_iterator_t* vector_get_iterator(vector_t* v);

/**
 * @brief 比较两个 vector_t（元素逐一比较）
 * @param a   第一个数组指针
 * @param b   第二个数组指针
 * @param cmp 元素比较函数
 * @return int 负数/0/正数 对应 a<b/a==b/a>b
 */
int private_vector_cmp(vector_t* a, vector_t* b, cmp_func cmp);

#endif
