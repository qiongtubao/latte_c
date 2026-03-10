/* object.c
 * 
 * Latte C 库组件实现
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

/**
 * latte_object_t 引用计数与释放；refcount 归 0 时联动 object_manager 释放。
 */
#include "object/object.h"
#include "object/object_manager.h"
#include "sds/sds.h"
#include "zmalloc/zmalloc.h"

latte_object_t *latte_object_new(unsigned type, void *ptr) {
    latte_object_t *o = zmalloc(sizeof(latte_object_t));
    if (!o)
        return NULL;
    o->type = type;
    o->lru = 0;
    o->refcount = 1;
    o->ptr = ptr;
    return o;
}

void latte_object_incr_ref_count(latte_object_t *o) {
    if (o)
        o->refcount++;
}

void latte_object_decr_ref_count(latte_object_t *o) {
    if (!o)
        return;
    if (o->refcount == 0)
        return;
    o->refcount--;
    if (o->refcount == 0) {
        if (o->type == OBJ_STRING) {
            if (o->ptr)
                sds_delete(o->ptr);
            zfree(o);
            return;
        }
        object_manager_release_wrapped(o);
    }
}
