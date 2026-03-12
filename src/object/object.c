/**
 * @file object.c
 * @brief latte_object_t 引用计数管理实现
 *
 * 提供 incr/decr 两个操作：
 * - incr：安全地将引用计数 +1
 * - decr：将引用计数 -1，归 0 时联动 object_manager 释放对象及内存
 */
#include "object/object.h"
#include "object/object_manager.h"

/**
 * @brief 增加对象引用计数
 * 若对象指针为 NULL 则安全跳过。
 * @param o 目标对象指针
 */
void latte_object_incr_ref_count(latte_object_t *o) {
    if (o)
        o->refcount++;
}

/**
 * @brief 减少对象引用计数，引用计数归 0 时释放对象
 *
 * 调用链：refcount-- → 若为 0 → object_manager_release_wrapped(o)
 * 注意：refcount 已为 0 时不再减少（防止下溢）。
 * @param o 目标对象指针，NULL 或 refcount 为 0 时安全返回
 */
void latte_object_decr_ref_count(latte_object_t *o) {
    if (!o)
        return;
    if (o->refcount == 0)
        return;
    o->refcount--;
    /* 引用计数归零，由 object_manager 负责释放 ptr 内容及对象本身 */
    if (o->refcount == 0)
        object_manager_release_wrapped(o);
}
