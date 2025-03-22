#ifndef LATTE_C_OBJ_H
#define LATTE_C_OBJ_H

#include "utils/atomic.h"
#include "dict/dict.h"



typedef struct latteObj {
    atomic_uint refcount;
    void* data;
    void (*free)(void* data);
} latteObj;

typedef void free_fn(void* data); 
latteObj* latteObjCreate(void* data, free_fn f);
int latteObjIncrRefCount(latteObj* obj);
int latteObjDecrRefCount(latteObj* obj);




#endif