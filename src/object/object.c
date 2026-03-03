/**
 * latte_object_t 引用计数与释放；refcount 归 0 时联动 object_manager 释放。
 */
#include "object/object.h"
#include "object/object_manager.h"

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
    if (o->refcount == 0)
        object_manager_release_wrapped(o);
}
