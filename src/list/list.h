/*
 * list.h - 双向链表头文件
 * 
 * Latte C 库基础组件：通用双向链表实现
 * 
 * 核心特性：
 * 1. 双向链表 (prev/next 指针)
 * 2. 可定制内存管理 (dup/free/match 回调)
 * 3. 支持迭代器遍历
 * 4. 头尾插入/删除
 * 5. 支持链表旋转操作
 * 
 * 主要结构：
 * - list_t: 链表主结构 (含头尾指针、长度、回调)
 * - list_node_t: 链表节点
 * - list_iterator_t: 迭代器
 * 
 * 应用场景：
 * - 事件队列
 * - 缓存链表
 * - 任务调度队列
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef __LATTE_LIST_H
#define __LATTE_LIST_H

#include "iterator/iterator.h"

/* 链表节点结构
 * 双向链表节点，包含前后指针和值
 */
typedef struct list_node_t {
    struct list_node_t *prev;  /* 前向指针 */
    struct list_node_t *next;  /* 后向指针 */
    void *value;               /* 节点值 (通用指针) */
} list_node_t;

/* 链表迭代器结构
 * 用于遍历链表
 */
typedef struct list_iterator_t {
    list_node_t *next;         /* 下一个节点指针 */
    int direction;             /* 遍历方向 (1=正向，-1=反向) */
} list_iterator_t;

/* 链表主结构
 * 包含头尾指针、长度、内存管理回调
 */
typedef struct list_t {
    list_node_t *head;         /* 头节点指针 */
    list_node_t *tail;         /* 尾节点指针 */
    void *(*dup)(void *ptr);   /* 节点值复制函数 (可选 NULL) */
    void (*free)(void *ptr);   /* 节点值释放函数 (可选 NULL) */
    int (*match)(void *ptr, void *key);  /* 节点值比较函数 (可选 NULL) */
    unsigned long len;         /* 链表长度 (节点数量) */
} list_t;


/* ========== 宏工具函数 ========== */

/* 获取链表长度 */
#define list_length(l) ((l)->len)

/* 获取头节点 */
#define list_first(l) ((l)->head)

/* 获取尾节点 */
#define list_last(l) ((l)->tail)

/* 获取前向节点 */
#define list_prev_node(n) ((n)->prev)

/* 获取后向节点 */
#define list_next_node(n) ((n)->next)

/* 获取节点值 */
#define list_node_value(n) ((n)->value)

/* 设置复制函数 */
#define list_set_dup_method(l,m) ((l)->dup = (m))

/* 设置释放函数 */
#define list_set_free_method(l,m) ((l)->free = (m))

/* 设置比较函数 */
#define list_set_match_method(l,m) ((l)->match = (m))

/* 获取复制函数 */
#define list_get_dup_method(l) ((l)->dup)

/* 获取释放函数 */
#define list_get_free_method(l) ((l)->free)

/* 获取比较函数 */
#define list_get_match_method(l) ((l)->match)

/* ========== 函数原型 ========== */

/* 创建新链表
 * 返回：成功返回链表指针，失败返回 NULL
 */
list_t *list_new(void);

/* 删除链表及其所有节点
 * 参数：list - 待删除的链表
 */
void list_delete(list_t *list);

/* 清空链表 (保留链表结构)
 * 参数：list - 待清空的链表
 */
void list_empty(list_t *list);

/* 在链表头部添加节点
 * 参数：list - 链表指针
 *       value - 节点值
 * 返回：添加后的链表指针
 */
list_t *list_add_node_head(list_t *list, void *value);

/* 在链表尾部添加节点
 * 参数：list - 链表指针
 *       value - 节点值
 * 返回：添加后的链表指针
 */
list_t *list_add_node_tail(list_t *list, void *value);

/* 在指定节点前后插入新节点
 * 参数：list - 链表指针
 *       old_node - 参考节点
 *       value - 新节点值
 *       after - 1=在 old_node 后，0=在 old_node 前
 * 返回：插入后的链表指针
 */
list_t *list_insert_node(list_t *list, list_node_t *old_node, void *value, int after);

/* 删除指定节点
 * 参数：list - 链表指针
 *       node - 待删除的节点
 */
void list_del_node(list_t *list, list_node_t *node);

/* 获取链表迭代器
 * 参数：list - 链表指针
 *       direction - 遍历方向 (1=从头到尾，-1=从尾到头)
 * 返回：迭代器指针
 */
list_iterator_t* list_get_iterator(list_t *list, int direction);

/* 迭代器获取下一个节点
 * 参数：iter - 迭代器指针
 * 返回：下一个节点，无节点返回 NULL
 */
list_node_t *list_next(list_iterator_t*iter);

/* 删除迭代器
 * 参数：iter - 待删除的迭代器
 */
void list_iterator_delete(list_iterator_t*iter);

/* 复制链表 (浅拷贝/深拷贝取决于 dup 函数)
 * 参数：orig - 原链表
 * 返回：新链表指针
 */
list_t *list_dup(list_t *orig);

/* 搜索指定键的节点
 * 参数：list - 链表指针
 *       key - 搜索键
 * 返回：找到的节点，未找到返回 NULL
 */
list_node_t *list_search_key(list_t *list, void *key);

/* 根据索引获取节点
 * 参数：list - 链表指针
 *       index - 索引 (0=头，-1=尾)
 * 返回：对应节点，索引无效返回 NULL
 */
list_node_t *list_index(list_t *list, long index);

/* 迭代器从头开始 (正向)
 * 参数：list - 链表指针
 *       li - 迭代器指针
 */
void list_rewind(list_t *list, list_iterator_t*li);

/* 迭代器从尾开始 (反向)
 * 参数：list - 链表指针
 *       li - 迭代器指针
 */
void list_rewind_tail(list_t *list, list_iterator_t*li);

/* 将尾节点移动到头部 (旋转操作)
 * 参数：list - 链表指针
 */
void list_rotate_tail_to_head(list_t *list);

/* 将头节点移动到尾部 (旋转操作)
 * 参数：list - 链表指针
 */
void list_rotate_head_to_tail(list_t *list);

