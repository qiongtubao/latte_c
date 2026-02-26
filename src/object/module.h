#ifndef __LATTE_OBJECT_MODULE_H
#define __LATTE_OBJECT_MODULE_H

#include "object.h"
#include <stdlib.h>
#include <string.h>
typedef struct latte_object_t redisObject;
/* Each module type implementation should export a set of methods in order
 * to serialize and deserialize the value in the RDB file, rewrite the AOF
 * log, create the digest for "DEBUG DIGEST", and free the value when a key
 * is deleted. */
typedef void *(*module_type_load_func)(struct RedisModuleIO *io, int encver);
typedef void (*module_type_save_func)(struct RedisModuleIO *io, void *value);
typedef int (*module_type_aux_load_func)(struct RedisModuleIO *rdb, int encver, int when);
typedef void (*module_type_aux_save_func)(struct RedisModuleIO *rdb, int when);
typedef void (*module_type_rewrite_func)(struct RedisModuleIO *io, struct redisObject *key, void *value);
typedef void (*module_type_digest_func)(struct RedisModuleDigest *digest, void *value);
typedef size_t (*module_type_mem_usage_func)(const void *value);
typedef void (*module_type_free_func)(void *value);
typedef size_t (*module_type_free_effort_func)(struct redisObject *key, const void *value);
typedef void (*module_type_unlink_func)(struct redisObject *key, void *value);
typedef void *(*module_type_copy_func)(struct redisObject *fromkey, struct redisObject *tokey, const void *value);
typedef int (*module_type_defrag_func )(struct RedisModuleDefragCtx *ctx, struct redisObject *key, void **value);

/* This callback type is called by moduleNotifyUserChanged() every time
 * a user authenticated via the module API is associated with a different
 * user or gets disconnected. This needs to be exposed since you can't cast
 * a function pointer to (void *). */
typedef void (*module_user_changed_func) (uint64_t client_id, void *privdata);

typedef struct module_type_t {
    uint64_t id;
    struct module_t* module;
    module_type_load_func load;
    module_type_save_func save;
    module_type_rewrite_func aoe_rewrite;
    module_type_mem_usage_func mem_usage;
    module_type_digest_func digest;
    module_type_free_func free;
    module_type_free_effort_func free_effort;
    module_type_unlink_func unlink;
    module_type_copy_func copy;
    module_type_defrag_func defrag;
    module_type_aux_load_func aux_load;
    module_type_aux_save_func aux_save;
    int aux_save_triggers;
    char name[10];
} module_type_t;

typedef struct module_value_t {
    module_type_t* type;
    void* value;
} module_value_t;
latte_object_t* module_object_new(module_type_t* mt, void* value);
#endif