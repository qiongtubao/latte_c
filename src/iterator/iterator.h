/**
 * @file iterator.h
 * @brief 通用迭代器接口
 *        提供统一的迭代器抽象，支持 has_next / next / release 操作，
 *        可用于链表、位图、字典等各类容器的遍历。
 *        同时提供键值对迭代器（latte_iterator_pair_t）用于 map 类容器。
 */
#ifndef __LATTE_ITERATOR_H
#define __LATTE_ITERATOR_H

#include <stdlib.h>
#include <stdbool.h>

/** @brief 迭代器前向声明 */
typedef struct latte_iterator_t latte_iterator_t;

/**
 * @brief 迭代器函数表（虚函数表）
 *        各容器实现时需填充此结构体的三个函数指针。
 *        TODO: 暂未增加 reset 函数。
 */
typedef struct latte_iterator_func {
    /**
     * @brief 判断是否还有下一个元素
     * @param iterator 当前迭代器
     * @return 有下一个返回 true，否则返回 false
     */
    bool (*has_next)(latte_iterator_t* iterator);

    /**
     * @brief 获取下一个元素并推进迭代器
     * @param iterator 当前迭代器
     * @return 下一个元素指针（具体类型由容器决定）
     */
    void* (*next)(latte_iterator_t* iterator);

    /**
     * @brief 释放迭代器自身占用的资源
     * @param iterator 要释放的迭代器
     */
    void (*release)(latte_iterator_t* iterator);
} latte_iterator_func;

/**
 * @brief 通用键值对结构体
 *        用于 map 类容器迭代时同时返回 key 和 value
 */
typedef struct latte_pair_t {
    void* key;    /**< 键指针 */
    void* value;  /**< 值指针 */
} latte_pair_t;

/**
 * @brief 创建键值对对象
 * @param key 键指针
 * @param val 值指针
 * @return 新建的 latte_pair_t 指针
 */
latte_pair_t* latte_pair_new(void* key, void* val);

/** @brief 获取键值对的 key */
#define latte_pair_key(pair) ((latte_pair_t*)pair)->key
/** @brief 获取键值对的 value */
#define latte_pair_value(pair) ((latte_pair_t*)pair)->value

/**
 * @brief 通用迭代器结构体
 */
typedef struct latte_iterator_t {
    latte_iterator_func* func;  /**< 函数表指针（has_next/next/release） */
    void* data;                 /**< 迭代器内部状态数据，由具体容器管理 */
} latte_iterator_t;

/**
 * @brief 带键值对的迭代器结构体
 *        继承 latte_iterator_t，额外内嵌一个 latte_pair_t 用于 map 类容器
 */
typedef struct latte_iterator_pair_t {
    latte_iterator_t sup;   /**< 基类迭代器（必须放在首位） */
    latte_pair_t pair;      /**< 当前迭代到的键值对 */
} latte_iterator_pair_t;

/** @brief 从 pair 迭代器获取当前 key */
#define iterator_pair_key(it)   ((latte_iterator_pair_t*)it)->pair.key
/** @brief 从 pair 迭代器获取当前 value */
#define iterator_pair_value(it) ((latte_iterator_pair_t*)it)->pair.value
/** @brief 从 pair 迭代器获取 pair 指针 */
#define iterator_pair_ptr(it)   &(((latte_iterator_pair_t*)it)->pair)

/**
 * @brief 创建通用迭代器
 * @param func 函数表指针
 * @param data 迭代器内部状态数据
 * @return 新建的 latte_iterator_t 指针
 */
latte_iterator_t* latte_iterator_new(latte_iterator_func* func, void* data);

/**
 * @brief 原地初始化通用迭代器（不分配内存）
 * @param it   已分配内存的迭代器指针
 * @param func 函数表指针
 * @param data 迭代器内部状态数据
 */
void latte_iterator_init(latte_iterator_t* it, latte_iterator_func* func, void* data);

/**
 * @brief 创建键值对迭代器
 * @param func 函数表指针
 * @param data 迭代器内部状态数据
 * @return 新建的 latte_iterator_t 指针（实际为 latte_iterator_pair_t）
 */
latte_iterator_t* latte_iterator_pair_new(latte_iterator_func* func, void* data);

/**
 * @brief 原地初始化键值对迭代器（不分配内存）
 * @param it   已分配内存的键值对迭代器指针
 * @param func 函数表指针
 * @param data 迭代器内部状态数据
 */
void latte_iterator_pair_init(latte_iterator_pair_t* it, latte_iterator_func* func, void* data);

/**
 * @brief 判断迭代器是否还有下一个元素
 * @param iterator 目标迭代器
 * @return 有下一个返回 true，否则返回 false
 */
bool latte_iterator_has_next(latte_iterator_t* iterator);

/**
 * @brief 获取迭代器的下一个元素并推进游标
 * @param iterator 目标迭代器
 * @return 下一个元素指针
 */
void* latte_iterator_next(latte_iterator_t* iterator);

/**
 * @brief 释放迭代器（调用 release 回调后释放结构体本身）
 * @param iterator 要释放的迭代器
 */
void latte_iterator_delete(latte_iterator_t* iterator);

#endif
