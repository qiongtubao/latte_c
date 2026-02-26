#ifndef __LATTE_LISTPACK_H
#define __LATTE_LISTPACK_H

#include <stdlib.h>
#include <stdint.h>
#include "iterator/iterator.h"

#define LIST_PACK_INTBUF_SIZE 21 /* 20 digits of -2^63 + 1 null term = 21. */


/* lpInsert() where argument possible values: */
//向前插入
#define LIST_PACK_BEFORE 0  
//向后插入
#define LIST_PACK_AFTER 1
//删除
#define LIST_PACK_REPLACE 2

/* 总的字符串 +  list 个数 +  entry(类型 + 数据长度 + 数据 + 前面所有的长度)  */

typedef struct list_pack_entry_t {
    unsigned char* sval;
    uint32_t slen;
    long long lval;
} list_pack_entry_t;

typedef unsigned char list_pack_t; 
list_pack_t* list_pack_new(size_t capacity);
void list_pack_delete(list_pack_t* lp);
/* 方便使用dict中free的方法*/
void list_pack_free_generic(void* lp);
/* 内存收缩 */
list_pack_t* list_pack_shrink_to_fit(list_pack_t* lp);
list_pack_t* list_pack_insert_string(list_pack_t* lp, unsigned char* s, uint32_t slen, 
                                        unsigned char* p, int where, unsigned char** newp);
list_pack_t*  list_pack_insert_integer(list_pack_t* lp, long long lval,
                                        unsigned char* p, int where, unsigned char** newp);
/*尾部追加*/
list_pack_t*  list_pack_append_string(list_pack_t* lp, unsigned char* s, uint32_t slen);

list_pack_t*  list_pack_append_integer(list_pack_t* lp, long long lval);

/*头部追加*/
list_pack_t* list_pack_prepend_string(list_pack_t* lp, unsigned char* s, uint32_t slen);
list_pack_t* list_pack_prepend_integer(list_pack_t* lp, long long lval);

/* 替换节点 */
list_pack_t* list_pack_replace_string(list_pack_t* lp, unsigned char** p, unsigned char* s, uint32_t slen);
list_pack_t* list_pack_replace_integer(list_pack_t* lp, unsigned char** p, long long lval);

/* 删除节点 */
list_pack_t* list_pack_remove(list_pack_t* lp, unsigned char *p, unsigned char **newp);
/* 删除连续的多个节点*/
list_pack_t* list_pack_remove_range_with_entry(list_pack_t* lp, unsigned char** p, unsigned long num);

/* 节点个数 */
unsigned long list_pack_length(list_pack_t* lp);
/* 下一个节点 */
unsigned char* list_pack_next(list_pack_t* lp, unsigned char* p);
/* 前一个节点 */
unsigned char* list_pack_prev(list_pack_t* lp, unsigned char* p);
/* 总字节数 */
size_t list_pack_bytes(list_pack_t* lp);
/* 最后一个节点 */
unsigned char* list_pack_last(list_pack_t* lp);
/* 获取节点值 */
unsigned char* list_pack_get_value(unsigned char* p, unsigned int *slen, long long *lval);

typedef struct latte_list_pack_value_t {
    unsigned char* sval;
    uint32_t slen;
    long long lval;
} latte_list_pack_value_t;


latte_iterator_t* list_pack_get_iterator(list_pack_t* lp, int where);



#endif