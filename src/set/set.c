/**
 * @file set.c
 * @brief 抽象Set集合数据结构实现文件
 * @details 提供了Set集合的多态接口，支持不同底层实现（AVL树、哈希表等）
 * @author latte_c
 * @date 2026-03-12
 */

#include "set.h"

/**
 * @brief 创建一个新的Set对象
 * @param type Set的操作函数表
 * @param data Set的底层数据结构实例
 * @return set_t* 返回新创建的Set对象，如果失败返回NULL
 */
set_t* set_new(set_func_t* type, void* data) {
    set_t* s = zmalloc(sizeof(set_t));
    s->data = data;
    s->type = type;
    return s;
}

/**
 * @brief 向Set中添加一个元素
 * @param s Set对象
 * @param key 要添加的元素指针
 * @return int 状态码：-1 表示失败，0 表示元素已存在，1 表示新添加成功
 */
// -1 失败  0 已经存在  1 新添加
int set_add(set_t* s, void* key) {
    return s->type->add(s, key);
}

/**
 * @brief 检查Set中是否包含指定的元素
 * @param s Set对象
 * @param key 要检查的元素指针
 * @return int 状态码：0 表示不存在，1 表示存在
 */
//0 不存在 1 存在
int set_contains(set_t* s, void* key) {
    return s->type->contains(s, key);
}

/**
 * @brief 从Set中移除指定的元素
 * @param s Set对象
 * @param key 要移除的元素指针
 * @return int 移除是否成功
 */
int set_remove(set_t* s, void* key) {
    return s->type->remove(s, key);
}

/**
 * @brief 获取Set中元素的数量
 * @param s Set对象
 * @return size_t 元素数量
 */
size_t set_size(set_t* s) {
    return s->type->size(s);
}

/**
 * @brief 释放Set对象及其底层结构
 * @param s Set对象
 */
void set_delete(set_t* s) {
    s->type->release(s);
}

/**
 * @brief 清空Set中的所有元素，但不释放Set对象本身
 * @param s Set对象
 */
void set_clear(set_t* s) {
    s->type->clear(s);
}

/**
 * @brief 获取Set的迭代器，用于遍历Set中的元素
 * @param s Set对象
 * @return latte_iterator_t* 返回创建的迭代器
 */
latte_iterator_t* set_get_iterator(set_t* s) {
    return s->type->getIterator(s);
}
