/**
 * @file iterator.c
 * @brief 通用迭代器实现模块
 *        提供多态迭代器接口，支持键值对和普通对象的统一迭代
 */
#include "iterator.h"
#include "zmalloc/zmalloc.h"

/**
 * @brief 创建一个新的键值对对象
 * @param key 键指针
 * @param val 值指针
 * @return latte_pair_t* 新创建的键值对指针
 */
latte_pair_t* latte_pair_new(void* key, void* val) {
    latte_pair_t* p = zmalloc(sizeof(latte_pair_t));
    p->key = key; /**< 设置键指针 */
    p->value = val; /**< 设置值指针 */
    return p;
}

/* ==================== 迭代器API ==================== */

/**
 * @brief 检查迭代器是否有下一个元素
 * @param iterator 迭代器指针
 * @return bool true表示有下一个元素，false表示已到末尾
 */
bool latte_iterator_has_next(latte_iterator_t* iterator) {
    return iterator->func->has_next(iterator);
}

/**
 * @brief 获取迭代器的下一个元素
 * @param iterator 迭代器指针
 * @return void* 下一个元素的指针，到达末尾时返回NULL
 */
void* latte_iterator_next(latte_iterator_t* iterator) {
    return iterator->func->next(iterator);
}

/**
 * @brief 释放迭代器占用的内存资源
 * @param iterator 要释放的迭代器指针
 */
void latte_iterator_delete(latte_iterator_t* iterator) {
    return iterator->func->release(iterator);
}

/**
 * @brief 初始化已分配的迭代器对象
 * @param it 待初始化的迭代器指针
 * @param func 函数指针表，定义迭代器的具体行为
 * @param data 迭代器关联的数据指针
 */
void latte_iterator_init(latte_iterator_t* it, latte_iterator_func* func, void* data) {
    it->func = func; /**< 设置函数指针表 */
    it->data = data; /**< 设置关联的数据指针 */
}

/**
 * @brief 创建并初始化一个新的迭代器
 * @param func 函数指针表，定义迭代器的具体行为
 * @param data 迭代器关联的数据指针
 * @return latte_iterator_t* 新创建的迭代器指针
 */
latte_iterator_t* latte_iterator_new(latte_iterator_func* func, void* data) {
    latte_iterator_t* it = zmalloc(sizeof(latte_iterator_t));
    latte_iterator_init(it, func, data);
    return it;
}

/**
 * @brief 初始化键值对迭代器
 * @param it 待初始化的键值对迭代器指针
 * @param func 函数指针表，定义迭代器的具体行为
 * @param data 迭代器关联的数据指针
 */
void latte_iterator_pair_init(latte_iterator_pair_t* it, latte_iterator_func* func, void* data) {
    latte_iterator_init(&it->sup, func, data); /**< 初始化父类迭代器 */
    it->pair.key = NULL; /**< 初始化键为空 */
    it->pair.value = NULL; /**< 初始化值为空 */
}

/**
 * @brief 创建并初始化一个新的键值对迭代器
 * @param func 函数指针表，定义迭代器的具体行为
 * @param data 迭代器关联的数据指针
 * @return latte_iterator_t* 新创建的迭代器指针（强制转换为基类）
 */
latte_iterator_t* latte_iterator_pair_new(latte_iterator_func* func, void* data) {
    latte_iterator_pair_t* it = zmalloc(sizeof(latte_iterator_pair_t));
    latte_iterator_pair_init(it, func, data);
    return (latte_iterator_t*)it; /**< 返回时强制转换为基类指针 */
}