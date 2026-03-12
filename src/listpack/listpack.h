#ifndef __LATTE_LISTPACK_H
#define __LATTE_LISTPACK_H

#include <stdlib.h>
#include <stdint.h>
#include "iterator/iterator.h"

/**
 * @brief 存储整数转换字符串时的缓冲区大小
 * -2^63占20位，加1个结束符null，共21字节
 */
#define LIST_PACK_INTBUF_SIZE 21 /* 20 digits of -2^63 + 1 null term = 21. */


/* lpInsert() where argument possible values: */
/** @brief 在指定节点前插入 */
//向前插入
#define LIST_PACK_BEFORE 0
/** @brief 在指定节点后插入 */
//向后插入
#define LIST_PACK_AFTER 1
/** @brief 替换指定节点 */
//删除
#define LIST_PACK_REPLACE 2

/* 总的字符串 +  list 个数 +  entry(类型 + 数据长度 + 数据 + 前面所有的长度)  */
/**
 * @brief listpack存储条目结构
 * 统一表示从listpack中解析出的节点数据。
 * 当sval不为NULL时，表示这是一个字符串节点；
 * 否则表示这是一个整数节点，值存储在lval中。
 */
typedef struct list_pack_entry_t {
    unsigned char* sval; /**< 字符串内容指针（如果非字符串则为NULL） */
    uint32_t slen;       /**< 字符串长度 */
    long long lval;      /**< 整数值（如果是整数节点） */
} list_pack_entry_t;

/**
 * @brief listpack数据结构，本质是一个连续的无符号字符数组
 */
typedef unsigned char list_pack_t;

/**
 * @brief 创建一个新的空listpack
 * @param capacity 预分配的初始容量，如果填0则分配最小所需空间
 * @return list_pack_t* 返回指向新创建listpack的指针
 */
list_pack_t* list_pack_new(size_t capacity);

/**
 * @brief 释放listpack占用的内存
 * @param lp 要释放的listpack
 */
void list_pack_delete(list_pack_t* lp);

/**
 * @brief 通用的释放函数，用于适配字典等需要回调释放的场景
 * @param lp 要释放的listpack指针
 */
/* 方便使用dict中free的方法*/
void list_pack_free_generic(void* lp);

/**
 * @brief 收缩listpack占用的内存至刚好适合当前数据大小
 * @param lp 要操作的listpack
 * @return list_pack_t* 重新分配内存后的listpack指针
 */
/* 内存收缩 */
list_pack_t* list_pack_shrink_to_fit(list_pack_t* lp);

/**
 * @brief 在listpack的指定位置插入一个字符串
 * @param lp 目标listpack
 * @param s 要插入的字符串指针
 * @param slen 字符串长度
 * @param p 插入的参考节点指针
 * @param where 插入位置标志：LIST_PACK_BEFORE, LIST_PACK_AFTER, LIST_PACK_REPLACE
 * @param newp 输出参数，返回新插入（或替换）节点的指针，若为NULL则不返回
 * @return list_pack_t* 返回修改后的listpack指针（可能因为内存重新分配而改变）
 */
list_pack_t* list_pack_insert_string(list_pack_t* lp, unsigned char* s, uint32_t slen,
                                        unsigned char* p, int where, unsigned char** newp);

/**
 * @brief 在listpack的指定位置插入一个整数
 * @param lp 目标listpack
 * @param lval 要插入的整数值
 * @param p 插入的参考节点指针
 * @param where 插入位置标志：LIST_PACK_BEFORE, LIST_PACK_AFTER, LIST_PACK_REPLACE
 * @param newp 输出参数，返回新插入（或替换）节点的指针，若为NULL则不返回
 * @return list_pack_t* 返回修改后的listpack指针（可能因为内存重新分配而改变）
 */
list_pack_t*  list_pack_insert_integer(list_pack_t* lp, long long lval,
                                        unsigned char* p, int where, unsigned char** newp);

/*尾部追加*/
/**
 * @brief 在listpack尾部追加一个字符串
 * @param lp 目标listpack
 * @param s 要追加的字符串指针
 * @param slen 字符串长度
 * @return list_pack_t* 返回修改后的listpack指针
 */
list_pack_t*  list_pack_append_string(list_pack_t* lp, unsigned char* s, uint32_t slen);

/**
 * @brief 在listpack尾部追加一个整数
 * @param lp 目标listpack
 * @param lval 要追加的整数值
 * @return list_pack_t* 返回修改后的listpack指针
 */
list_pack_t*  list_pack_append_integer(list_pack_t* lp, long long lval);

/*头部追加*/
/**
 * @brief 在listpack头部前置一个字符串
 * @param lp 目标listpack
 * @param s 要前置的字符串指针
 * @param slen 字符串长度
 * @return list_pack_t* 返回修改后的listpack指针
 */
list_pack_t* list_pack_prepend_string(list_pack_t* lp, unsigned char* s, uint32_t slen);

/**
 * @brief 在listpack头部前置一个整数
 * @param lp 目标listpack
 * @param lval 要前置的整数值
 * @return list_pack_t* 返回修改后的listpack指针
 */
list_pack_t* list_pack_prepend_integer(list_pack_t* lp, long long lval);

/* 替换节点 */
/**
 * @brief 替换listpack中指定的节点为新的字符串
 * @param lp 目标listpack
 * @param p 指向要被替换的节点的指针地址，函数执行后会更新为新节点的地址
 * @param s 新的字符串指针
 * @param slen 字符串长度
 * @return list_pack_t* 返回修改后的listpack指针
 */
list_pack_t* list_pack_replace_string(list_pack_t* lp, unsigned char** p, unsigned char* s, uint32_t slen);

/**
 * @brief 替换listpack中指定的节点为新的整数
 * @param lp 目标listpack
 * @param p 指向要被替换的节点的指针地址，函数执行后会更新为新节点的地址
 * @param lval 新的整数值
 * @return list_pack_t* 返回修改后的listpack指针
 */
list_pack_t* list_pack_replace_integer(list_pack_t* lp, unsigned char** p, long long lval);

/* 删除节点 */
/**
 * @brief 删除listpack中的指定节点
 * @param lp 目标listpack
 * @param p 要删除的节点指针
 * @param newp 输出参数，返回被删除节点之后的下一个节点的指针
 * @return list_pack_t* 返回修改后的listpack指针
 */
list_pack_t* list_pack_remove(list_pack_t* lp, unsigned char *p, unsigned char **newp);

/* 删除连续的多个节点*/
/**
 * @brief 从指定节点开始，删除连续的num个节点
 * @param lp 目标listpack
 * @param p 指向要开始删除的节点的指针地址，执行后会更新为删除区间后的第一个节点指针
 * @param num 要删除的节点数量
 * @return list_pack_t* 返回修改后的listpack指针
 */
list_pack_t* list_pack_remove_range_with_entry(list_pack_t* lp, unsigned char** p, unsigned long num);

/* 节点个数 */
/**
 * @brief 获取listpack中存储的节点数量
 * @param lp 目标listpack
 * @return unsigned long 节点数量
 */
unsigned long list_pack_length(list_pack_t* lp);

/* 下一个节点 */
/**
 * @brief 遍历获取当前节点的下一个节点
 * @param lp 目标listpack
 * @param p 当前节点指针
 * @return unsigned char* 下一个节点指针，如果已到末尾则返回NULL
 */
unsigned char* list_pack_next(list_pack_t* lp, unsigned char* p);

/* 前一个节点 */
/**
 * @brief 遍历获取当前节点的前一个节点
 * @param lp 目标listpack
 * @param p 当前节点指针
 * @return unsigned char* 前一个节点指针，如果已到头部则返回NULL
 */
unsigned char* list_pack_prev(list_pack_t* lp, unsigned char* p);

/* 总字节数 */
/**
 * @brief 获取整个listpack结构占用的总内存字节数
 * @param lp 目标listpack
 * @return size_t 字节数
 */
size_t list_pack_bytes(list_pack_t* lp);

/* 最后一个节点 */
/**
 * @brief 获取listpack中的最后一个节点
 * @param lp 目标listpack
 * @return unsigned char* 最后一个节点的指针，如果listpack为空则返回NULL
 */
unsigned char* list_pack_last(list_pack_t* lp);

/* 获取节点值 */
/**
 * @brief 解析并获取指定节点中存储的值
 * @param p 节点指针
 * @param slen 输出参数，如果节点是字符串，保存字符串长度
 * @param lval 输出参数，如果节点是整数，保存整数值
 * @return unsigned char* 如果是字符串节点，返回字符串内存的指针；如果是整数节点则返回NULL
 */
unsigned char* list_pack_get_value(unsigned char* p, unsigned int *slen, long long *lval);

/**
 * @brief 封装从listpack中取出的值
 */
typedef struct latte_list_pack_value_t {
    unsigned char* sval;
    uint32_t slen;
    long long lval;
} latte_list_pack_value_t;

/**
 * @brief 获取listpack的迭代器
 * @param lp 目标listpack
 * @param where 迭代的起始位置及方向：通常传入0表示正向迭代，1表示反向迭代
 * @return latte_iterator_t* 返回创建好的迭代器
 */
latte_iterator_t* list_pack_get_iterator(list_pack_t* lp, int where);



#endif