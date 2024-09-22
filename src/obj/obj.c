#include "obj.h"
#include "dict/dict.h"
#include "zmalloc/zmalloc.h"

latteObj* latteObjCreate(void* data, free_fn f) {
    latteObj* o = zmalloc(sizeof(latteObj));
    o->data = data;
    atomic_store(&o->refcount, 1);
    o->free = f;
    return o;
}

#define OBJ_SHARED_REFCOUNT INT_MAX     /* Global object never destroyed. */
#define OBJ_STATIC_REFCOUNT (INT_MAX-1) /* Object allocated in the stack. */
#define OBJ_FIRST_SPECIAL_REFCOUNT OBJ_STATIC_REFCOUNT


int latteObjDecrRefCount(latteObj* obj) {
    int count = atomic_load(&obj->refcount);
    if (count != OBJ_SHARED_REFCOUNT && count > 0) {
        if (1 == atomic_fetch_sub(&obj->refcount, 1)) {
            obj->free(obj->data);
            obj->data = NULL;
            zfree(obj);
            return 0;
        }
    } else if(count <=0) {
        printf("decrRefCount against refcount = %d", count);
        exit(0);
    }
     
    return obj->refcount;
}

int latteObjIncrRefCount(latteObj* obj) {
    if (obj->refcount < OBJ_FIRST_SPECIAL_REFCOUNT) {
        atomic_fetch_add(&obj->refcount, 1);
    } else {
        if (obj->refcount == OBJ_SHARED_REFCOUNT) {

        } else if (obj->refcount == OBJ_STATIC_REFCOUNT) {
            printf("You tried to retain an object allocated in the stack\n");
            exit(0);
        }
    }
    return obj->refcount;
}