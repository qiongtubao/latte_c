/**
 * Object 管理：按类型名（字符串）注册，类型 id 内部递增分配；创建时自动把 type_id 写入 object 首字节，释放时根据 object 的 type 查找并释放。
 *
 * 约定：由 manager 创建或加载的 object，其首字节必须保留给 type_id（由 manager 在 create/load 时写入）。
 * 即 create_fn/load_fn 返回的指针指向的缓冲区，第一个字节会被 manager 覆盖为 type_id。
 */
#ifndef __LATTE_OBJECT_MANAGER_H__
#define __LATTE_OBJECT_MANAGER_H__

#include <stddef.h>
#include <stdint.h>
#include "object/object.h"
#include "odb/odb.h" /* oio 类型 */
#include "error/error.h"

/** 从 object 指针读取 type_id（首字节） */
#define OBJECT_TYPE_ID(obj)  (*(uint8_t*)(obj))

/* 每种类型的回调：创建、释放、保存到 oio、从 oio 加载、计算方法（如 hash/校验） */
typedef latte_object_t* (*object_create_fn)(void);
typedef void  (*object_release_fn)(latte_object_t *obj);
typedef latte_error_t   (*object_save_fn)(oio *o, latte_object_t *obj);
typedef latte_error_t* (*object_load_fn)(oio *o, latte_object_t **out_obj);
typedef uint64_t (*object_calc_fn)(latte_object_t *obj);

extern struct object_manager_t global_object_manager;

/** 创建管理器；用完后 object_manager_free */
void global_object_manager_init(void);

/**
 * 用字符串名称注册类型：创建、释放、保存、加载、计算方法；
 * 类型 id 由内部递增分配（0, 1, 2, ...）。calc 可为 NULL。
 * 返回 0 成功，-1 失败（如名称已注册、槽位满或参数无效）。
 */
int object_manager_register(const char *type_name,
    object_create_fn create_fn,
    object_release_fn release_fn,
    object_save_fn save_fn,
    object_load_fn load_fn,
    object_calc_fn calc_fn);

/**
 * 按类型名创建 object；创建后自动将 type_id 写入 object 首字节。失败返回 NULL。
 */
latte_object_t* object_manager_create_object( const char *type_name);

/**
 * 按类型名创建包装为 latte_object_t 的对象：o->type = type_id，o->ptr = create_fn()，
 * o->refcount = 1，o->mgr = mgr；latte_object_decr_ref_count 归 0 时会联动本 manager 释放。
 * 失败返回 NULL。
 */
latte_object_t* object_manager_create_wrapped(const char *type_name);

/**
 * 释放 object：根据 object 首字节的 type_id 查找对应类型的 release_fn 并调用，无需传入类型名或 id。
 */
void object_manager_release_object(latte_object_t *obj);

/**
 * 释放由 object_manager_create_wrapped 创建的对象：根据 o->type 释放 o->ptr，再释放 o 自身。
 * 由 latte_object_decr_ref_count（refcount 归 0 时）或调用方直接调用。
 */
void object_manager_release_wrapped( latte_object_t *o);

/**
 * 将 object 保存到 odb：从 object 首字节读 type_id，写 type_id(u8) 后写 payload。
 * 返回 0 成功，-1 失败。
 */
latte_error_t* object_manager_save( oio *o, const latte_object_t *obj);

/**
 * 从 odb 加载 object：先读 type_id，再调用该类型的 load_fn，并将 type_id 写回 object 首字节。
 * 成功时 *out_obj 为对象（调用方用 object_manager_release_object 释放）；失败返回 -1。
 */
latte_error_t* object_manager_load( oio *o, void **out_obj);

/** 按 type_id 取注册时的类型名；未注册或无效返回 NULL（只读，勿 free） */
const char* object_manager_get_type_name(uint8_t type_id);

/** 对 object 执行该类型的计算方法（从 object 首字节读 type_id）。未注册 calc 则返回 0。 */
uint64_t object_manager_calc(const void *obj);

#endif /* __LATTE_OBJECT_MANAGER_H__ */
