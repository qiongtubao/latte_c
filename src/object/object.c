#include "object.h"
#include "sds/sds.h"
#include "dict/dict.h"
#include "log/log.h"
#include "debug/latte_debug.h"
#include <string.h>
#include "io/io.h"

latte_object_t* latte_object_new(int type, void* ptr) {
    latte_object_t* o = zmalloc(sizeof(*o));
    o->type = type;
    o->encoding = OBJ_ENCODING_RAW;
    o->ptr = ptr;
    o->refcount = 1;
    return o;
}

void latte_object_decr_ref_count(latte_object_t* o) {
    if (o->refcount == 1) {
        object_type_t* type = vector_get(global_object_factory.object_types, o->type);
        type->free(o);
        zfree(o);
    } else {
        if (o->refcount <= 0) latte_panic("decrRefCount against refcount <= 0");
        if (o->refcount != OBJ_SHARED_REFCOUNT) o->refcount--;
    }
}

void latte_object_incr_ref_count(latte_object_t* o) {
    if (o->refcount < OBJ_FIRST_SPECIAL_REFCOUNT) {
        o->refcount++;
    } else {
        if (o->refcount == OBJ_SHARED_REFCOUNT) {
            /* Nothing to do: this refcount is immutable. */
        } else if (o->refcount == OBJ_STATIC_REFCOUNT) {
            // serverPanic("You tried to retain an object allocated in the stack");
        }
    }
}

latte_object_t* make_object_shared(latte_object_t* o) {
    o->refcount = OBJ_SHARED_REFCOUNT;
    return o;
}


char* str_encoding(int encoding) {
    switch(encoding) {
        case OBJ_ENCODING_RAW: return "raw";
        case OBJ_ENCODING_INT: return "int";
        case OBJ_ENCODING_HT: return "hashtable";
        case OBJ_ENCODING_QUICKLIST: return "quicklist";
        case OBJ_ENCODING_ZIPLIST: return "ziplist";
        case OBJ_ENCODING_INTSET: return "intset";
        case OBJ_ENCODING_SKIPLIST: return "skiplist";
        case OBJ_ENCODING_EMBSTR: return "embstr";
        case OBJ_ENCODING_STREAM: return "stream";
        default: return "unknown";
    }
}



uint64_t object_type_hash_callback(const void *key) {
    return dict_gen_case_hash_function((unsigned char*)key, strlen((char*)key));
}

int object_type_compare_callback(dict_t*privdata, const void *key1, const void *key2) {
    int l1,l2;
    DICT_NOTUSED(privdata);
    l1 = strlen((char*) key1);
    l2 = strlen((char*) key2);
    if(l1 != l2) return 0;
    return memcmp(key1, key2,l1) == 0;
}

static dict_func_t object_type_name = {
    object_type_hash_callback,
    NULL,
    NULL,
    object_type_compare_callback,
    NULL,
    NULL,
    NULL
};


void object_init() {
    global_object_factory.object_types = vector_new();
    global_object_factory.type_names = dict_new(&object_type_name);

    register_object_type(&object_string_type);
    register_object_type(&object_list_type);
    register_object_type(&object_set_type);
    register_object_type(&object_zset_type);
    register_object_type(&object_hash_type);
    register_object_type(&object_stream_type);
}

int register_object_type(object_type_t* type) {
    dict_entry_t* entry = dict_find(global_object_factory.type_names, type->name);
    if (entry != NULL) return -1;
    size_t index = vector_size(global_object_factory.object_types);
    if (vector_push(global_object_factory.object_types, type) != 0) {
        return -2;
    }
    if (dict_add(global_object_factory.type_names, type->name, (void*)index) != 0) {
        vector_pop(global_object_factory.object_types);
        return -3;
    }
    return 0;
}

size_t object_compute_size(latte_object_t *o, size_t sample) {
    object_type_t* type = vector_get(global_object_factory.object_types, o->type);
    return type->compute(o, sample);
}

void object_free(latte_object_t* o) {
    object_type_t* type = vector_get(global_object_factory.object_types, o->type);
    return type->free(o);
}

latte_object_t* object_load(io_t* io, vector_t* types, int* size) {
    uint8_t byte;
    io_read(io, &byte, 1);
    object_type_t* otype = vector_get(types, byte);
    latte_assert(otype != NULL);
    return otype->load(io, size);
}

int object_save(latte_object_t* o, io_t* io) {
    unsigned char typechar = o->type;
    io_write(io, &typechar, 1);
    object_type_t* type = vector_get(global_object_factory.object_types, o->type);
    return type->save(o, io);
}