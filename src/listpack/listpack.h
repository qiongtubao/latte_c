#ifndef __LATTE_LISTPACK_H
#define __LATTE_LISTPACK_H

#include <stdlib.h>
#include <stdint.h>

#define LP_INTBUF_SIZE 21 /* 20 digits of -2^63 + 1 null term = 21. */


/* lpInsert() where argument possible values: */
//向前插入
#define LP_BEFORE 0  
//向后插入
#define LP_AFTER 1
//删除
#define LP_REPLACE 2

/* 总的字符串 +  list 个数 +  entry(type + len + data + total_entry_bit)  */

typedef struct list_pack_entry_t {
    unsigned char* sval;
    uint32_t slen;
    long long lval;
} list_pack_entry_t;

typedef unsigned char list_pack_t; 
list_pack_t* list_pack_new(size_t capacity);
void list_pack_delete(list_pack_t* lp);
void list_pack_free_generic(void* lp);
unsigned char* list_pack_shrink_to_fit(list_pack_t* lp);
unsigned char* list_pack_insert_string(list_pack_t* lp, unsigned char* s, uint32_t slen, 
                                        unsigned char* p, int where, list_pack_t** newp);
unsigned char*  list_pack_insert_integer(list_pack_t* lp, long long lval,
                                        unsigned char* p, int where, list_pack_t** newp);
unsigned char*  list_pack_append(list_pack_t* lp, unsigned char* s, uint32_t slen);
unsigned char*  list_pack_append_integer(list_pack_t* lp, long long lval);


#endif