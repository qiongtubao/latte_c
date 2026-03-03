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

void global_object_manager_init(void) {
    struct object_manager_t *m = mgr();
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

latte_error_t* object_manager_load(oio *o, void **out_obj) {
    struct object_manager_t *m = mgr();
    if (!o || !out_obj) return error_new(CInvalidArgument, "object_manager_load", "null");
    *out_obj = NULL;
    uint8_t type_id = 0;
    if (odb_read_u8(o, &type_id) != 1) return error_new(CIOError, "object_manager_load", "read type_id");
    if ((unsigned)type_id >= OBJECT_MANAGER_MAX_TYPES) return error_new(CInvalidArgument, "object_manager_load", "invalid type_id");
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
