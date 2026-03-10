/* list.c - 双向链表实现文件
 * 
 * Latte C 库基础组件：通用双向链表实现
 * 基于 Redis adlist.c 改编
 * 
 * 核心特性：
 * 1. 双向链表 (prev/next 指针)
 * 2. 可定制内存管理 (dup/free/match 回调)
 * 3. O(1) 头尾插入/删除
 * 4. 支持迭代器遍历
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#include <stdlib.h>
#include "list.h"
#include "zmalloc/zmalloc.h"

/*
 * list_new - 创建新链表
 * 
 * 功能：
 * - 分配链表结构
 * - 初始化头尾指针为 NULL
 * - 初始化长度为 0
 * - 清空所有回调函数
 * 
 * 注意：
 * - 创建的链表需要由 user 负责释放节点值
 * - 或设置 list_set_free_method 自动释放
 * 
 * 返回：成功返回链表指针，失败返回 NULL
 */
/*
 * list_new - 创建新链表
 */
list_t *list_new(void)
{
    struct list_t *list;

    if ((list = zmalloc(sizeof(*list))) == NULL)
        return NULL;
    
    list->head = list->tail = NULL;  /* 初始化头尾指针 */
    list->len = 0;                    /* 长度清零 */
    list->dup = NULL;                 /* 清空复制回调 */
    list->free = NULL;                /* 清空释放回调 */
    list->match = NULL;               /* 清空比较回调 */
    
    return list;
}

/*
 * list_empty - 清空链表 (保留链表结构)
 * 
 * 功能：
 * - 遍历所有节点
 * - 调用 free 回调释放节点值 (如果已设置)
 * - 释放节点本身
 * - 重置头尾指针和长度
 * 
 * 注意：
 * - 不释放链表结构本身
 * - 调用 list_delete 前应先清空
 */
/*
 * list_empty - 清空链表
 */
void list_empty(list_t *list)
{
    unsigned long len;
    list_node_t *current, *next;

    current = list->head;
    len = list->len;
    
    /* 遍历所有节点并释放 */
    while(len--) {
        next = current->next;
        
        /* 如果设置了释放回调，释放节点值 */
        if (list->free) 
            list->free(current->value);
        
        /* 释放节点本身 */
        zfree(current);
        
        current = next;
    }
    
    /* 重置链表状态 */
    list->head = list->tail = NULL;
    list->len = 0;
}

/*
 * list_for_each_delete - 条件删除节点
 * 
 * 功能：
 * - 遍历链表
 * - 对每个节点调用 need_delete 回调
 * - 如果返回 true，删除该节点
 * 
 * 参数：
 * - l: 链表指针
 * - need_delete: 判断函数，返回 true 表示需要删除
 * 
 * 返回：删除的节点数量
 */
int list_for_each_delete(list_t* l, int (*need_delete)(void* value)) {
    list_node_t* node = l->head;
    list_node_t* next = NULL;
    int deleted = 0;
    
    while (node) {
        next = node->next;  /* 保存下一个节点 */
        
        /* 如果满足删除条件 */
        if (need_delete(node->value)) {
            /*
             * list_del_node - 删除指定节点
             */
            list_del_node(l, node);  /* 删除当前节点 */
            deleted++;
        }
        
        node = next;  /* 移动到下一个节点 */
    }
    
    return deleted;
}

/*
 * list_delete - 删除整个链表
 * 
 * 功能：
 * - 先清空所有节点
 * - 再释放链表结构本身
 * 
 * 注意：
 * - 此函数不会失败
 * - 调用前确保所有资源已正确处理
 */
/*
 * list_delete - 删除整个链表
 */
void list_delete(list_t *list)
{
    /*
     * list_empty - 清空链表
     */
    list_empty(list);  /* 清空所有节点 */
    zfree(list);       /* 释放链表结构 */
}

/*
 * list_add_node_head - 在链表头部添加节点
 * 
 * 参数：
 * - list: 链表指针
 * - value: 节点值
 * 
 * 功能：
 * - 分配新节点
 * - 调用 dup 回调复制值 (如果已设置)
 * - 插入到链表头部
 * - 更新头尾指针
 * 
 * 返回：添加后的链表指针
 */
/*
 * list_add_node_head - 在头部添加节点
 */
list_t *list_add_node_head(list_t *list, void *value)
{
    list_node_t *node;

    /* 分配新节点 */
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    
    /* 复制值 (如果设置了 dup 回调) */
    if (list->dup)
        node->value = list->dup(value);
    else
        node->value = value;
    
    /* 插入到头部 */
    node->next = list->head;
    node->prev = NULL;
    
    /* 更新头指针 */
    if (list->head)
        list->head->prev = node;
    else
        list->tail = node;  /* 如果原链表为空，更新尾指针 */
    
    list->head = node;
    list->len++;
    
    return list;
}

/*
 * list_add_node_tail - 在链表尾部添加节点
 * 
 * 参数：
 * - list: 链表指针
 * - value: 节点值
 * 
 * 功能：
 * - 分配新节点
 * - 调用 dup 回调复制值 (如果已设置)
 * - 插入到链表尾部
 * - 更新头尾指针
 * 
 * 返回：添加后的链表指针
 */
/*
 * list_add_node_tail - 在尾部添加节点
 */
list_t *list_add_node_tail(list_t *list, void *value)
{
    list_node_t *node;

    /* 分配新节点 */
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    
    /* 复制值 (如果设置了 dup 回调) */
    if (list->dup)
        node->value = list->dup(value);
    else
        node->value = value;
    
    /* 插入到尾部 */
    node->next = NULL;
    node->prev = list->tail;
    
    /* 更新尾指针 */
    if (list->tail)
        list->tail->next = node;
    else
        list->head = node;  /* 如果原链表为空，更新头指针 */
    
    list->tail = node;
    list->len++;
    
    return list;
}

/*
 * list_insert_node - 在指定节点前后插入新节点
 * 
 * 参数：
 * - list: 链表指针
 * - old_node: 参考节点
 * - value: 新节点值
 * - after: 1=在 old_node 后插入，0=在 old_node 前插入
 * 
 * 功能：
 * - 分配新节点
 * - 复制值 (如果设置了 dup 回调)
 * - 调整指针连接
 * - 更新头尾指针 (如果插入到头或尾)
 * 
 * 返回：插入后的链表指针
 */
/*
 * list_insert_node - 在指定位置插入节点
 */
list_t *list_insert_node(list_t *list, list_node_t *old_node, void *value, int after)
{
    list_node_t *node;

    /* 分配新节点 */
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    
    /* 复制值 (如果设置了 dup 回调) */
    if (list->dup)
        node->value = list->dup(value);
    else
        node->value = value;
    
    /* 插入到 old_node 后 */
    if (after) {
        node->next = old_node->next;
        node->prev = old_node;
        
        if (old_node->next)
            old_node->next->prev = node;
        else
            list->tail = node;  /* 如果插入到尾部，更新尾指针 */
        
        old_node->next = node;
    } else {  /* 插入到 old_node 前 */
        node->prev = old_node->prev;
        node->next = old_node;
        
        if (old_node->prev)
            old_node->prev->next = node;
        else
            list->head = node;  /* 如果插入到头部，更新头指针 */
        
        old_node->prev = node;
    }
    
    list->len++;
    
    return list;
}

/*
 * list_del_node - 删除指定节点
 * 
 * 参数：
 * - list: 链表指针
 * - node: 待删除的节点
 * 
 * 功能：
 * - 调整前后节点的指针
 * - 释放节点值 (如果设置了 free 回调)
 * - 释放节点本身
 * - 更新头尾指针 (如果删除的是头或尾)
 * - 更新长度
 */
/*
 * list_del_node - 删除指定节点
 */
void list_del_node(list_t *list, list_node_t *node)
{
    /* 调整前驱节点的 next 指针 */
    if (node->prev)
        node->prev->next = node->next;
    else
        list->head = node->next;  /* 删除的是头节点 */
    
    /* 调整后继节点的 prev 指针 */
    if (node->next)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;  /* 删除的是尾节点 */
    
    /* 释放节点值 */
    if (list->free)
        list->free(node->value);
    
    /* 释放节点本身 */
    zfree(node);
    
    /* 更新长度 */
    list->len--;
}

/*
 * list_get_iterator - 获取链表迭代器
 * 
 * 参数：
 * - list: 链表指针
 * - direction: 遍历方向 (1=正向，-1=反向)
 * 
 * 返回：迭代器指针
 */
list_iterator_t* list_get_iterator(list_t *list, int direction)
{
    list_iterator_t *iter;
    
    if ((iter = zmalloc(sizeof(*iter))) == NULL)
        return NULL;
    
    /* 根据方向设置起始节点 */
    iter->next = (direction == 1) ? list->head : list->tail;
    iter->direction = direction;
    
    return iter;
}

/*
 * list_next - 迭代器获取下一个节点
 * 
 * 参数：
 * - iter: 迭代器指针
 * 
 * 返回：下一个节点，无节点返回 NULL
 * 
 * 注意：
 * - 正向遍历时返回当前节点，然后移动到下一个
 * - 反向遍历时返回当前节点，然后移动到前一个
 */
list_node_t *list_next(list_iterator_t *iter)
{
    list_node_t *current = iter->next;
    
    if (current != NULL) {
        /* 根据方向移动迭代器 */
        if (iter->direction == 1)
            iter->next = current->next;
        else
            iter->next = current->prev;
    }
    
    return current;
}

/*
 * list_iterator_delete - 删除迭代器
 * 
 * 参数：
 * - iter: 待删除的迭代器
 */
/*
 * list_iterator_delete - 删除迭代器
 */
void list_iterator_delete(list_iterator_t *iter)
{
    zfree(iter);
}

/*
 * list_dup - 复制链表
 * 
 * 参数：
 * - orig: 原链表
 * 
 * 返回：新链表指针
 * 
 * 功能：
 * - 创建新链表
 * - 遍历原链表
 * - 对每个节点调用 dup 回调复制值
 * - 添加到新链表尾部
 */
/*
 * list_dup - 复制链表
 */
list_t *list_dup(list_t *orig)
{
    list_t *copy;
    list_node_t *node;
    
    /*
     * list_new - 创建新链表
     */
    if ((copy = list_new()) == NULL)
        return NULL;
    
    /* 设置与原链表相同的回调 */
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;
    
    /* 遍历原链表并复制节点 */
    node = orig->head;
    while (node) {
        /*
         * list_add_node_tail - 在尾部添加节点
         */
        list_add_node_tail(copy, node->value);
        node = node->next;
    }
    
    return copy;
}

/*
 * list_search_key - 搜索指定键的节点
 * 
 * 参数：
 * - list: 链表指针
 * - key: 搜索键
 * 
 * 返回：找到的节点，未找到返回 NULL
 * 
 * 功能：
 * - 遍历链表
 * - 对每个节点调用 match 回调
 * - 返回第一个匹配的节点
 */
/*
 * list_search_key - 搜索指定键的节点
 */
list_node_t *list_search_key(list_t *list, void *key)
{
    list_node_t *node;
    
    if (list->match == NULL)
        return NULL;  /* 未设置比较函数 */
    
    node = list->head;
    while (node) {
        if (list->match(node->value, key))
            return node;
        
        node = node->next;
    }
    
    return NULL;
}

/*
 * list_index - 根据索引获取节点
 * 
 * 参数：
 * - list: 链表指针
 * - index: 索引 (0=头，-1=尾)
 * 
 * 返回：对应节点，索引无效返回 NULL
 * 
 * 注意：
 * - 正数索引从头开始
 * - 负数索引从尾开始
 */
/*
 * list_index - 根据索引获取节点
 */
list_node_t *list_index(list_t *list, long index)
{
    list_node_t *node;
    
    if (index >= 0) {
        /* 从头开始遍历 */
        if ((unsigned long)index >= list->len)
            return NULL;
        
        node = list->head;
        while (index--)
            node = node->next;
    } else {
        /* 从尾开始遍历 */
        index = -index - 1;
        if ((unsigned long)index >= list->len)
            return NULL;
        
        node = list->tail;
        while (index--)
            node = node->prev;
    }
    
    return node;
}

/*
 * list_rewind - 迭代器从头开始 (正向)
 * 
 * 参数：
 * - list: 链表指针
 * - li: 迭代器指针
 */
/*
 * list_rewind - 迭代器从头开始
 */
void list_rewind(list_t *list, list_iterator_t *li)
{
    li->next = list->head;
    li->direction = 1;
}

/*
 * list_rewind_tail - 迭代器从尾开始 (反向)
 * 
 * 参数：
 * - list: 链表指针
 * - li: 迭代器指针
 */
/*
 * list_rewind_tail - 迭代器从尾开始
 */
void list_rewind_tail(list_t *list, list_iterator_t *li)
{
    li->next = list->tail;
    li->direction = -1;
}

/*
 * list_rotate_tail_to_head - 将尾节点移动到头部
 * 
 * 参数：
 * - list: 链表指针
 * 
 * 功能：
 * - 将尾节点移到头部
 * - 更新头尾指针
 * - 适用场景：LRU 缓存访问后移动
 */
void list_rotate_tail_to_head(list_t *list)
{
    if (list->len <= 1)
        return;  /* 只有一个或零个节点，无需移动 */
    
    list_node_t *tail = list->tail;
    
    /* 移除尾节点 */
    list->tail = tail->prev;
    list->tail->next = NULL;
    
    /* 插入到头部 */
    tail->next = list->head;
    tail->prev = NULL;
    list->head->prev = tail;
    list->head = tail;
}

/*
 * list_rotate_head_to_tail - 将头节点移动到尾部
 * 
 * 参数：
 * - list: 链表指针
 * 
 * 功能：
 * - 将头节点移到尾部
 * - 更新头尾指针
 */
void list_rotate_head_to_tail(list_t *list)
{
    if (list->len <= 1)
        return;  /* 只有一个或零个节点，无需移动 */
    
    list_node_t *head = list->head;
    
    /* 移除头节点 */
    list->head = head->next;
    list->head->prev = NULL;
    
    /* 插入到尾部 */
    head->prev = list->tail;
    head->next = NULL;
    list->tail->next = head;
    list->tail = head;
}
