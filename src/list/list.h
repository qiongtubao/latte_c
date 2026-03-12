#ifndef __LATTE_LIST_H
#define __LATTE_LIST_H

#include "iterator/iterator.h"

/**
 * @brief 双向链表节点结构体
 */
typedef struct list_node_t {
    struct list_node_t *prev; /**< 指向前一个节点的指针 */
    struct list_node_t *next; /**< 指向后一个节点的指针 */
    void *value;              /**< 节点存储的值指针 */
} list_node_t;

/**
 * @brief 链表迭代器结构体
 */
typedef struct list_iterator_t {
    list_node_t *next; /**< 迭代器的下一个节点 */
    int direction;     /**< 迭代方向：AL_START_HEAD（从头到尾）或 AL_START_TAIL（从尾到头） */
} list_iterator_t;

/**
 * @brief 双向链表结构体
 */
typedef struct list_t {
    list_node_t *head; /**< 链表头节点指针 */
    list_node_t *tail; /**< 链表尾节点指针 */
    void *(*dup)(void *ptr); /**< 节点值复制函数指针，用于复制链表时复制节点值 */
    void (*free)(void *ptr); /**< 节点值释放函数指针，用于释放节点时释放节点值 */
    int (*match)(void *ptr, void *key); /**< 节点值匹配函数指针，用于查找指定值的节点 */
    unsigned long len; /**< 链表包含的节点数量 */
} list_t;


/* Functions implemented as macros */
/** @brief 获取链表长度宏 */
#define list_length(l) ((l)->len)
/** @brief 获取链表头节点宏 */
#define list_first(l) ((l)->head)
/** @brief 获取链表尾节点宏 */
#define list_last(l) ((l)->tail)
/** @brief 获取节点的前一个节点宏 */
#define list_prev_node(n) ((n)->prev)
/** @brief 获取节点的后一个节点宏 */
#define list_next_node(n) ((n)->next)
/** @brief 获取节点存储的值宏 */
#define list_node_value(n) ((n)->value)

/** @brief 设置链表节点值复制函数宏 */
#define list_set_dup_method(l,m) ((l)->dup = (m))
/** @brief 设置链表节点值释放函数宏 */
#define list_set_free_method(l,m) ((l)->free = (m))
/** @brief 设置链表节点值匹配函数宏 */
#define list_set_match_method(l,m) ((l)->match = (m))

/** @brief 获取链表节点值复制函数宏 */
#define list_get_dup_method(l) ((l)->dup)
/** @brief 获取链表节点值释放函数宏 */
#define list_get_free_method(l) ((l)->free)
/** @brief 获取链表节点值匹配函数宏 */
#define list_get_match_method(l) ((l)->match)

/* Prototypes */

/**
 * @brief 创建一个新的空链表
 * @return list_t* 指向新创建的链表的指针
 */
list_t *list_new(void);

/**
 * @brief 删除链表并释放所有节点和链表结构体本身占用的内存
 * @param list 指向要删除的链表的指针
 */
void list_delete(list_t *list);

/**
 * @brief 清空链表，释放所有节点但不释放链表结构体本身
 * @param list 指向要清空的链表的指针
 */
void list_empty(list_t *list);

/**
 * @brief 在链表头部添加一个新节点
 * @param list 目标链表指针
 * @param value 要添加的值指针
 * @return list_t* 返回修改后的链表指针
 */
list_t *list_add_node_head(list_t *list, void *value);

/**
 * @brief 在链表尾部添加一个新节点
 * @param list 目标链表指针
 * @param value 要添加的值指针
 * @return list_t* 返回修改后的链表指针
 */
list_t *list_add_node_tail(list_t *list, void *value);

/**
 * @brief 在链表中指定的节点前或后插入一个新节点
 * @param list 目标链表指针
 * @param old_node 参考节点指针
 * @param value 要插入的值指针
 * @param after 插入位置标志：非0表示在old_node之后插入，0表示在old_node之前插入
 * @return list_t* 返回修改后的链表指针
 */
list_t *list_insert_node(list_t *list, list_node_t *old_node, void *value, int after);

/**
 * @brief 从链表中删除指定的节点
 * @param list 目标链表指针
 * @param node 要删除的节点指针
 */
void list_del_node(list_t *list, list_node_t *node);

/**
 * @brief 创建一个链表迭代器
 * @param list 目标链表指针
 * @param direction 迭代方向，可选值为 AL_START_HEAD 或 AL_START_TAIL
 * @return list_iterator_t* 返回创建的迭代器指针
 */
list_iterator_t*list_get_iterator(list_t *list, int direction);

/**
 * @brief 获取迭代器的下一个节点
 * @param iter 迭代器指针
 * @return list_node_t* 返回下一个节点的指针，如果没有则返回NULL
 */
list_node_t *list_next(list_iterator_t*iter);

/**
 * @brief 删除链表迭代器，释放内存
 * @param iter 要删除的迭代器指针
 */
void list_iterator_delete(list_iterator_t*iter);

/**
 * @brief 复制整个链表
 * @param orig 原始链表指针
 * @return list_t* 返回复制出的新链表指针，如果内存分配失败返回NULL
 */
list_t *list_dup(list_t *orig);

/**
 * @brief 查找链表中包含指定键值的节点
 * @param list 目标链表指针
 * @param key 要查找的键值指针
 * @return list_node_t* 返回找到的节点指针，如果未找到返回NULL
 */
list_node_t *list_search_key(list_t *list, void *key);

/**
 * @brief 根据索引获取链表节点
 * @param list 目标链表指针
 * @param index 节点索引（支持负数，-1表示最后一个节点，-2表示倒数第二个，依此类推）
 * @return list_node_t* 返回指定索引的节点指针，如果越界则返回NULL
 */
list_node_t *list_index(list_t *list, long index);

/**
 * @brief 重置迭代器，使其指向链表头部
 * @param list 目标链表指针
 * @param li 要重置的迭代器指针
 */
void list_rewind(list_t *list, list_iterator_t*li);

/**
 * @brief 重置迭代器，使其指向链表尾部
 * @param list 目标链表指针
 * @param li 要重置的迭代器指针
 */
void list_rewind_tail(list_t *list, list_iterator_t*li);

/**
 * @brief 将链表尾部节点移动到链表头部
 * @param list 目标链表指针
 */
void list_rotate_tail_to_head(list_t *list);

/**
 * @brief 将链表头部节点移动到链表尾部
 * @param list 目标链表指针
 */
void list_rotate_head_to_tail(list_t *list);

/**
 * @brief 将一个链表合并到另一个链表的尾部，并清空原链表
 * @param l 目标链表指针
 * @param o 要合并的源链表指针（合并后将变为空链表）
 */
void list_join(list_t *l, list_t *o);

/**
 * @brief 遍历链表，并根据条件删除节点
 * @param l 目标链表指针
 * @param need_delete 指向条件函数的指针，如果该函数返回非0值，则删除该节点
 * @return int 总是返回0
 */
int list_for_each_delete(list_t* l, int (*need_delete)(void* value));




// option
/** @brief 迭代器选项：从头开始迭代 */
#define LIST_ITERATOR_OPTION_HEAD 0
/** @brief 迭代器选项：从尾开始迭代 */
#define LIST_ITERATOR_OPTION_TAIL (1 << 0)
/** @brief 迭代器选项：迭代结束后自动删除链表（并释放值，如果有free函数） */
#define LIST_ITERATOR_OPTION_DELETE_LIST (1 << 1)

/**
 * @brief 判断多态迭代器是否有下一个元素
 */
bool protected_latte_list_iterator_has_next(latte_iterator_t* iterator);

/**
 * @brief 获取多态迭代器的下一个元素的值
 */
void* protected_latte_list_iterator_next(latte_iterator_t* iterator);

/**
 * @brief 删除多态迭代器
 */
void protected_latte_list_iterator_delete(latte_iterator_t* iterator);

/**
 * @brief 获取链表的多态迭代器
 * @param l 目标链表指针
 * @param opt 迭代器选项位掩码，如 LIST_ITERATOR_OPTION_HEAD 等
 * @return latte_iterator_t* 返回创建好的迭代器
 */
latte_iterator_t* list_get_latte_iterator(list_t* l, int opt);

/**
 * @brief 将指定节点移动到链表头部
 * @param l 目标链表指针
 * @param node 要移动的节点指针
 */
void  list_move_head(list_t* l, list_node_t* node);

/**
 * @brief 从链表中移除指定节点，但不释放节点存储的值（由调用者负责）
 * @param l 目标链表指针
 * @param node 要移除的节点指针
 * @return void* 返回被移除节点存储的值指针
 */
void* list_remove(list_t* l, list_node_t* node);

/**
 * @brief 从链表头部弹出一个节点的值
 * @param l 目标链表指针
 * @return void* 返回被弹出节点的值指针
 */
void* list_lpop(list_t* l) ;

/**
 * @brief 从链表尾部弹出一个节点的值
 * @param l 目标链表指针
 * @return void* 返回被弹出节点的值指针
 */
void* list_rpop(list_t* l) ;

/** @brief 迭代方向宏定义：从头部开始 */
#define AL_START_HEAD 0
/** @brief 迭代方向宏定义：从尾部开始 */
#define AL_START_TAIL 1
#endif
