/**
 * @file object_manager.c
 * @brief Object 管理实现：全局 global_object_manager，按类型名注册，类型 id 递增分配
 */
#include "object/object_manager.h"
#include "odb/odb.h"
#include "sds/sds.h"
#include "zmalloc/zmalloc.h"
#include <string.h>

#define OBJECT_MANAGER_MAX_TYPES 256

/** @brief 单个类型注册项，存储类型名及其回调函数 */
typedef struct {
    int used;                   /**< 该槽位是否已使用 */
    sds name;                   /**< 类型名称（sds 字符串） */
    object_create_fn create_fn; /**< 创建对象的回调 */
    object_release_fn release_fn; /**< 释放对象的回调 */
    object_save_fn save_fn;     /**< 序列化回调 */
    object_load_fn load_fn;     /**< 反序列化回调 */
    object_calc_fn calc_fn;     /**< 计算方法回调（可为 NULL） */
} type_entry_t;

struct object_manager_t {
    type_entry_t types[OBJECT_MANAGER_MAX_TYPES];
};

struct object_manager_t global_object_manager;

/**
 * @brief 获取全局 object_manager 单例指针
 * @return 指向 global_object_manager 的指针
 */
static struct object_manager_t* mgr(void) {
    return &global_object_manager;
}

/**
 * @brief 按类型名在 manager 中查找类型 id
 * @param m         object_manager 指针
 * @param type_name 要查找的类型名称
 * @return 找到返回类型 id（>=0），未找到或参数无效返回 -1
 */
static int find_type_id_by_name(struct object_manager_t *m, const char *type_name) {
    if (!m || !type_name) return -1;
    for (unsigned i = 0; i < OBJECT_MANAGER_MAX_TYPES; i++) {
        if (m->types[i].used && m->types[i].name && strcmp(m->types[i].name, type_name) == 0)
            return (int)i;
    }
    return -1;
}

/**
 * @brief 查找 manager 中第一个空闲槽位
 * @param m object_manager 指针
 * @return 找到返回槽位索引，无空闲或参数无效返回 -1
 */
static int first_free_slot(struct object_manager_t *m) {
    if (!m) return -1;
    for (unsigned i = 0; i < OBJECT_MANAGER_MAX_TYPES; i++) {
        if (!m->types[i].used) return (int)i;
    }
    return -1;
}

/**
 * @brief 释放 manager 中所有已注册类型的名称字符串
 * @param m object_manager 指针
 */
static void release_all_types(struct object_manager_t *m) {
    for (unsigned i = 0; i < OBJECT_MANAGER_MAX_TYPES; i++) {
        if (m->types[i].name) {
            sds_delete(m->types[i].name);
            m->types[i].name = NULL;
        }
    }
}

/**
 * @brief 初始化全局 object_manager（重复调用会先释放已有注册再清零）
 */
void global_object_manager_init(void) {
    struct object_manager_t *m = mgr();
    release_all_types(m);
    memset(m, 0, sizeof(*m));
}

/**
 * @brief 释放全局 object_manager 内所有 type name，程序退出或重 init 前调用
 */
void global_object_manager_free(void) {
    struct object_manager_t *m = mgr();
    release_all_types(m);
    memset(m, 0, sizeof(*m));
}

/**
 * @brief 注册对象类型：按类型名分配 id，绑定创建/释放/保存/加载/计算回调
 * @param type_name  类型名称字符串（不可重复注册）
 * @param create_fn  创建对象的回调
 * @param release_fn 释放对象的回调
 * @param save_fn    序列化回调
 * @param load_fn    反序列化回调
 * @param calc_fn    计算方法回调（可为 NULL）
 * @return 0 成功，-1 失败（参数无效/重复注册/槽位满）
 */
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

/**
 * @brief 按类型名创建对象，自动写入 type_id 到 obj->type
 * @param type_name 已注册的类型名称
 * @return 新建对象指针；类型未注册或创建失败返回 NULL
 */
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

/**
 * @brief 同 object_manager_create_object，为 wrapped 模式创建对象
 * @param type_name 已注册的类型名称
 * @return 新建对象指针；失败返回 NULL
 */
latte_object_t* object_manager_create_wrapped(const char *type_name) {
    return object_manager_create_object(type_name);
}

/**
 * @brief 内部释放 wrapped 对象：先调用 release_fn 释放 payload，再 zfree 对象本身
 * @param o 要释放的 latte_object_t 指针（可为 NULL）
 */
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

/**
 * @brief 释放由 object_manager_create_object 创建的对象
 * @param obj 要释放的对象指针（可为 NULL）
 */
void object_manager_release_object(latte_object_t *obj) {
    if (!obj) return;
    release_wrapped(obj);
}

/**
 * @brief 释放由 object_manager_create_wrapped 创建的对象
 * @param o 要释放的 latte_object_t 指针（可为 NULL）
 */
void object_manager_release_wrapped(latte_object_t *o) {
    release_wrapped(o);
}

/**
 * @brief 将对象序列化保存到 odb：写入 type_id(u8) 后写 payload
 * @param o   目标 oio 输出流
 * @param obj 要保存的对象（首字节须为 type_id）
 * @return NULL 成功；失败返回错误指针
 */
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

/**
 * @brief 从 odb 加载对象：读取 type_id，通过 id_map 映射后调用 load_fn
 * @param o       源 oio 输入流
 * @param out_obj 输出参数，成功时指向加载的对象
 * @param id_map  可选的 id 映射表（NULL 则直接使用读取的 type_id）
 * @return NULL 成功；失败返回错误指针
 */
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

/**
 * @brief 按 type_id 获取已注册类型名（只读，勿 free）
 * @param type_id 类型 id
 * @return 类型名字符串；未注册或无效返回 NULL
 */
const char* object_manager_get_type_name(uint8_t type_id) {
    struct object_manager_t *m = mgr();
    if ((unsigned)type_id >= OBJECT_MANAGER_MAX_TYPES) return NULL;
    type_entry_t *e = &m->types[type_id];
    return (e->used && e->name) ? e->name : NULL;
}

/**
 * @brief 对对象执行其类型的计算方法（从 obj->type 读取 type_id）
 * @param obj 要计算的对象
 * @return 计算结果；未注册 calc_fn 或参数无效返回 0
 */
uint64_t object_manager_calc(const void *obj) {
    struct object_manager_t *m = mgr();
    const latte_object_t *o = (const latte_object_t*)obj;
    if (!o) return 0;
    uint8_t type_id = (uint8_t)o->type;
    if ((unsigned)type_id >= OBJECT_MANAGER_MAX_TYPES) return 0;
    type_entry_t *e = &m->types[type_id];
    return (e->calc_fn) ? e->calc_fn((latte_object_t*)o) : 0;
}

/**
 * @brief 将类型注册表保存到 odb
 *        格式：类型数量(u32) + 每个类型[id(u8) + name(string)]
 * @param o 目标 oio 输出流
 * @return NULL 成功；失败返回错误指针
 */
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

/**
 * @brief 从 odb 加载类型注册表并建立 id 映射
 *        读取所有类型 id/name，检查当前 manager 是否已注册，输出 id_map
 * @param o      源 oio 输入流
 * @param id_map 输出 256 大小数组：索引=保存时 id，值=当前 id；未注册置 0xFF
 * @return NULL 成功；失败返回错误指针
 */
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

