/* object_manager.c
 * 
 * Latte C 库组件实现
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

/**
 * Object 管理实现：全局 global_object_manager，按类型名注册，类型 id 递增分配。
 */
#include "object/object_manager.h"
#include "odb/odb.h"
#include "sds/sds.h"
#include "zmalloc/zmalloc.h"
#include <string.h>

#define OBJECT_MANAGER_MAX_TYPES 256

typedef struct {
    int used;
    sds name;
    object_create_fn create_fn;
    object_release_fn release_fn;
    object_save_fn save_fn;
    object_load_fn load_fn;
    object_calc_fn calc_fn;
} type_entry_t;

struct object_manager_t {
    type_entry_t types[OBJECT_MANAGER_MAX_TYPES];
};

struct object_manager_t global_object_manager;

static struct object_manager_t* mgr(void) {
    return &global_object_manager;
}

static int find_type_id_by_name(struct object_manager_t *m, const char *type_name) {
    if (!m || !type_name) return -1;
    for (unsigned i = 0; i < OBJECT_MANAGER_MAX_TYPES; i++) {
        if (m->types[i].used && m->types[i].name && strcmp(m->types[i].name, type_name) == 0)
            return (int)i;
    }
    return -1;
}

static int first_free_slot(struct object_manager_t *m) {
    if (!m) return -1;
    for (unsigned i = 0; i < OBJECT_MANAGER_MAX_TYPES; i++) {
        if (!m->types[i].used) return (int)i;
    }
    return -1;
}

static void release_all_types(struct object_manager_t *m) {
    for (unsigned i = 0; i < OBJECT_MANAGER_MAX_TYPES; i++) {
        if (m->types[i].name) {
            sds_delete(m->types[i].name);
            m->types[i].name = NULL;
        }
    }
}

void global_object_manager_init(void) {
    struct object_manager_t *m = mgr();
    release_all_types(m);
    memset(m, 0, sizeof(*m));
}

void global_object_manager_free(void) {
    struct object_manager_t *m = mgr();
    release_all_types(m);
    memset(m, 0, sizeof(*m));
}

int object_manager_register(const char *type_name,
    object_create_fn create_fn,
    object_release_fn release_fn,
    object_save_fn save_fn,
    object_load_fn load_fn,
    object_calc_fn calc_fn)
{
    struct object_manager_t *m = mgr();
    if (!type_name || !create_fn || !release_fn || !save_fn || !load_fn)
        return -1;
    if (find_type_id_by_name(m, type_name) >= 0)
        return -1;
    int slot = first_free_slot(m);
    if (slot < 0) return -1;
    type_entry_t *e = &m->types[slot];
    e->name = sds_new(type_name);
    if (!e->name) return -1;
    e->used = 1;
    e->create_fn = create_fn;
    e->release_fn = release_fn;
    e->save_fn = save_fn;
    e->load_fn = load_fn;
    e->calc_fn = calc_fn;
    return 0;
}

latte_object_t* object_manager_create_object(const char *type_name) {
    struct object_manager_t *m = mgr();
    int id = find_type_id_by_name(m, type_name);
    if (id < 0) return NULL;
    type_entry_t *e = &m->types[id];
    if (!e->create_fn) return NULL;
    latte_object_t *obj = e->create_fn();
    if (!obj) return NULL;
    obj->type = (unsigned)id;
    obj->refcount = 1;
    return obj;
}

latte_object_t* object_manager_create_wrapped(const char *type_name) {
    return object_manager_create_object(type_name);
}

static void release_wrapped(latte_object_t *o) {
    struct object_manager_t *m = mgr();
    if (!o) return;
    if (o->ptr) {
        uint8_t type_id = (uint8_t)o->type;
        if ((unsigned)type_id < OBJECT_MANAGER_MAX_TYPES) {
            type_entry_t *e = &m->types[type_id];
            if (e->used && e->release_fn)
                e->release_fn(o);
        }
    }
    zfree(o);
}

void object_manager_release_object(latte_object_t *obj) {
    if (!obj) return;
    release_wrapped(obj);
}

void object_manager_release_wrapped(latte_object_t *o) {
    release_wrapped(o);
}

latte_error_t* object_manager_save(oio *o, const latte_object_t *obj) {
    struct object_manager_t *m = mgr();
    if (!o || !obj) return error_new(CInvalidArgument, "object_manager_save", "null");
    uint8_t type_id = (uint8_t)obj->type;
    if ((unsigned)type_id >= OBJECT_MANAGER_MAX_TYPES) return error_new(CInvalidArgument, "object_manager_save", "invalid type_id");
    type_entry_t *e = &m->types[type_id];
    if (!e->used || !e->save_fn) return error_new(CInvalidArgument, "object_manager_save", "no save_fn");
    if (odb_write_u8(o, type_id) != 1) return error_new(CIOError, "object_manager_save", "write type_id");
    latte_error_t err = e->save_fn(o, (latte_object_t*)obj);
    if (!error_is_ok(&err)) return error_new(err.code, "object_manager_save", "save_fn failed");
    return NULL;
}

latte_error_t* object_manager_load(oio *o, void **out_obj, const uint8_t id_map[256]) {
    struct object_manager_t *m = mgr();
    if (!o || !out_obj) return error_new(CInvalidArgument, "object_manager_load", "null");
    *out_obj = NULL;
    
    /* 读取保存时的 type_id */
    uint8_t saved_id = 0;
    if (odb_read_u8(o, &saved_id) != 1) return error_new(CIOError, "object_manager_load", "read type_id");
    
    /* 如果提供了 id_map，使用映射转换；否则直接使用读取的 type_id */
    uint8_t type_id = saved_id;
    if (id_map != NULL) {
        type_id = id_map[saved_id];
        if (type_id == 0xFF) {
            return error_new(CCorruption, "object_manager_load", 
                "no mapping for saved_id %u", saved_id);
        }
    }
    
    if ((unsigned)type_id >= OBJECT_MANAGER_MAX_TYPES) {
        return error_new(CInvalidArgument, "object_manager_load", "invalid type_id");
    }
    
    type_entry_t *e = &m->types[type_id];
    if (!e->used || !e->load_fn) return error_new(CInvalidArgument, "object_manager_load", "no load_fn");
    
    latte_object_t *obj = NULL;
    latte_error_t *err = e->load_fn(o, &obj);
    if (err) return err;
    if (!obj) return error_new(CCorruption, "object_manager_load", "load_fn returned no object");
    
    obj->type = (unsigned)type_id;
    obj->refcount = 1;
    *out_obj = obj;
    return NULL;
}

const char* object_manager_get_type_name(uint8_t type_id) {
    struct object_manager_t *m = mgr();
    if ((unsigned)type_id >= OBJECT_MANAGER_MAX_TYPES) return NULL;
    type_entry_t *e = &m->types[type_id];
    return (e->used && e->name) ? e->name : NULL;
}

uint64_t object_manager_calc(const void *obj) {
    struct object_manager_t *m = mgr();
    const latte_object_t *o = (const latte_object_t*)obj;
    if (!o) return 0;
    uint8_t type_id = (uint8_t)o->type;
    if ((unsigned)type_id >= OBJECT_MANAGER_MAX_TYPES) return 0;
    type_entry_t *e = &m->types[type_id];
    return (e->calc_fn) ? e->calc_fn((latte_object_t*)o) : 0;
}

latte_error_t* object_manager_save_registry(oio *o) {
    struct object_manager_t *m = mgr();
    if (!o) return error_new(CInvalidArgument, "object_manager_save_registry", "null oio");
    
    /* 统计已注册的类型数量 */
    uint32_t count = 0;
    for (unsigned i = 0; i < OBJECT_MANAGER_MAX_TYPES; i++) {
        if (m->types[i].used) count++;
    }
    
    /* 写入类型数量 */
    if (odb_write_u32(o, count) != 1) {
        return error_new(CIOError, "object_manager_save_registry", "write count");
    }
    
    /* 写入每个类型的 id 和 name */
    for (unsigned i = 0; i < OBJECT_MANAGER_MAX_TYPES; i++) {
        if (m->types[i].used && m->types[i].name) {
            /* 写入 type_id */
            if (odb_write_u8(o, (uint8_t)i) != 1) {
                return error_new(CIOError, "object_manager_save_registry", "write type_id");
            }
            /* 写入 type_name */
            size_t name_len = sds_len(m->types[i].name);
            if (odb_write_string(o, m->types[i].name, name_len) != 1) {
                return error_new(CIOError, "object_manager_save_registry", "write type_name");
            }
        }
    }
    
    return NULL;
}

latte_error_t* object_manager_load_registry(oio *o, uint8_t id_map[256]) {
    struct object_manager_t *m = mgr();
    if (!o || !id_map) return error_new(CInvalidArgument, "object_manager_load_registry", "null");
    
    /* 初始化 id_map 为 0xFF（表示未映射） */
    memset(id_map, 0xFF, 256);
    
    /* 读取类型数量 */
    uint32_t count = 0;
    if (odb_read_u32(o, &count) != 1) {
        return error_new(CIOError, "object_manager_load_registry", "read count");
    }
    
    /* 读取每个类型的 id 和 name，并建立映射 */
    for (uint32_t i = 0; i < count; i++) {
        uint8_t saved_id = 0;
        if (odb_read_u8(o, &saved_id) != 1) {
            return error_new(CIOError, "object_manager_load_registry", "read saved_id");
        }
        
        sds type_name = odb_read_string(o);
        if (!type_name) {
            return error_new(CIOError, "object_manager_load_registry", "read type_name");
        }
        
        /* 在当前 manager 中查找该类型名对应的 type_id */
        int current_id = find_type_id_by_name(m, type_name);
        
        if (current_id < 0) {
            /* 类型未注册，返回错误 */
            sds_delete(type_name);
            return error_new(CNotFound, "object_manager_load_registry", "type not registered");
        }
        
        sds_delete(type_name);
        
        /* 建立映射：保存时的 id -> 当前的 id */
        id_map[saved_id] = (uint8_t)current_id;
    }
    
    return NULL;
}

