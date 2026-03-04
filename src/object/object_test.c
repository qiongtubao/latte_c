/**
 * object_manager 单测：全局 manager、注册、创建、释放、保存/加载、计算、引用计数。
 */
#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include <string.h>
#include "object/object_manager.h"
#include "odb/odb.h"
#include "sds/sds.h"
#include "zmalloc/zmalloc.h"
#include "error/error.h"

#define TEST_TYPE_NAME "blob"

typedef struct {
    sds s;
} blob_t;

static latte_object_t* blob_create(void) {
    latte_object_t *o = (latte_object_t*)zmalloc(sizeof(latte_object_t));
    if (!o) return NULL;
    blob_t *b = (blob_t*)zmalloc(sizeof(blob_t));
    if (!b) { zfree(o); return NULL; }
    b->s = sds_empty();
    o->ptr = b;
    o->type = 0;
    o->refcount = 0;
    o->lru = 0;
    return o;
}

static void blob_release(latte_object_t *o) {
    if (!o || !o->ptr) return;
    blob_t *b = (blob_t*)o->ptr;
    if (b->s) sds_delete(b->s);
    zfree(b);
}

static latte_error_t blob_save(oio *o, latte_object_t *obj) {
    blob_t *b = (blob_t*)obj->ptr;
    if (!b) return (latte_error_t){CInvalidArgument, NULL};
    return odb_write_string(o, b->s, sds_len(b->s)) ? (latte_error_t){COk, NULL} : (latte_error_t){CIOError, NULL};
}

static latte_error_t* blob_load(oio *o, latte_object_t **out_obj) {
    blob_t *b = (blob_t*)zmalloc(sizeof(blob_t));
    if (!b) return error_new(CIOError, "blob_load", "alloc");
    b->s = odb_read_string(o);
    if (!b->s) { zfree(b); return error_new(CIOError, "blob_load", "read string"); }
    latte_object_t *obj = (latte_object_t*)zmalloc(sizeof(latte_object_t));
    if (!obj) { sds_delete(b->s); zfree(b); return error_new(CIOError, "blob_load", "alloc object"); }
    obj->ptr = b;
    obj->type = 0;
    obj->refcount = 0;
    obj->lru = 0;
    *out_obj = obj;
    return NULL;
}

static uint64_t blob_calc(latte_object_t *obj) {
    blob_t *b = (blob_t*)obj->ptr;
    if (!b || !b->s) return 0;
    sds s = b->s;
    uint64_t h = 0;
    for (size_t i = 0; i < sds_len(s); i++)
        h = h * 31 + (unsigned char)((char*)s)[i];
    return h;
}

static int test_register_create_release(void) {
    global_object_manager_init();
    assert(object_manager_register(TEST_TYPE_NAME, blob_create, blob_release, blob_save, blob_load, blob_calc) == 0);
    latte_object_t *obj = object_manager_create_object(TEST_TYPE_NAME);
    assert(obj != NULL);
    assert(obj->type == 0);
    object_manager_release_object(obj);
    return 1;
}

static int test_save_load(void) {
    global_object_manager_init();
    assert(object_manager_register(TEST_TYPE_NAME, blob_create, blob_release, blob_save, blob_load, blob_calc) == 0);
    latte_object_t *orig = object_manager_create_object(TEST_TYPE_NAME);
    assert(orig != NULL);
    blob_t *b0 = (blob_t*)orig->ptr;
    sds_delete(b0->s);
    b0->s = sds_new("hello");
    oio *o = odb_oio_create_buffer();
    assert(o != NULL);
    latte_error_t *err = object_manager_save(o, orig);
    assert(err == NULL);
    odb_oio_buffer_rewind(o);
    void *loaded = NULL;
    err = object_manager_load(o, &loaded);
    assert(err == NULL);
    assert(loaded != NULL);
    latte_object_t *lo = (latte_object_t*)loaded;
    assert(lo->type == 0);
    assert(object_manager_get_type_name((uint8_t)lo->type) != NULL);
    assert(strcmp(object_manager_get_type_name((uint8_t)lo->type), TEST_TYPE_NAME) == 0);
    blob_t *b = (blob_t*)lo->ptr;
    assert(sds_len(b->s) == 5);
    assert(memcmp(b->s, "hello", 5) == 0);
    object_manager_release_object(lo);
    object_manager_release_object(orig);
    odb_oio_free(o);
    return 1;
}

static int test_calc(void) {
    global_object_manager_init();
    assert(object_manager_register(TEST_TYPE_NAME, blob_create, blob_release, blob_save, blob_load, blob_calc) == 0);
    latte_object_t *obj = object_manager_create_object(TEST_TYPE_NAME);
    assert(obj != NULL);
    blob_t *b = (blob_t*)obj->ptr;
    sds_delete(b->s);
    b->s = sds_new("ab");
    uint64_t h = object_manager_calc(obj);
    assert(h != 0);
    object_manager_release_object(obj);
    return 1;
}

static int test_wrapped_refcount(void) {
    global_object_manager_init();
    assert(object_manager_register(TEST_TYPE_NAME, blob_create, blob_release, blob_save, blob_load, blob_calc) == 0);
    latte_object_t *o = object_manager_create_wrapped(TEST_TYPE_NAME);
    assert(o != NULL);
    assert(o->type == 0);
    assert(o->refcount == 1);
    assert(o->ptr != NULL);
    latte_object_incr_ref_count(o);
    assert(o->refcount == 2);
    latte_object_incr_ref_count(o);
    assert(o->refcount == 3);
    latte_object_decr_ref_count(o);
    assert(o->refcount == 2);
    latte_object_decr_ref_count(o);
    assert(o->refcount == 1);
    latte_object_decr_ref_count(o);
    return 1;
}

int main(void) {
    test_cond("object_manager register create release", test_register_create_release() == 1);
    test_cond("object_manager save load", test_save_load() == 1);
    test_cond("object_manager calc", test_calc() == 1);
    test_cond("object_manager wrapped + incr/decr refcount", test_wrapped_refcount() == 1);
    test_report();
    global_object_manager_free();
    return 0;
}
