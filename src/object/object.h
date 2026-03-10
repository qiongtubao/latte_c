/*
 * object.h - object 模块头文件
 * 
 * Latte C 库组件
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef __LATTE_OBJECT_H__
#define __LATTE_OBJECT_H__

#define LRU_BITS 24
#define OBJ_REFCOUNT_BITS 32
#define OBJ_STRING 1
#define OBJ_MODULE 2

struct object_manager_t;

typedef struct latte_object_t {
    unsigned type:8;
    unsigned lru:LRU_BITS; /* LRU time (relative to global lru_clock) or
                            * LFU data (least significant 8 bits frequency
                            * and most significant 16 bits access time). */
    unsigned refcount : OBJ_REFCOUNT_BITS;
    void *ptr;
} latte_object_t;

void latte_object_incr_ref_count(latte_object_t *o);
latte_object_t *latte_object_new(unsigned type, void *ptr);
void latte_object_decr_ref_count(latte_object_t *o);
#endif