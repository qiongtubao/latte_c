#include "assert.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "zmalloc.h"
typedef struct VectorClock {
    char length;
    union {
        long long single;
        void *multi;
    };
} VectorClock;
typedef struct Meta {
    char gid:8;
    long long timestamp;//8
} __attribute__ ((packed, aligned(1))) Meta;
#define copyMeta1(a, b)  \
    do { \
        a->gid = b.gid;\
        a->timestamp = b.timestamp;\
    } while (0);
#define copyMeta2(a, b)  \
    do { \
        a->gid = b->gid;\
        a->timestamp = b->timestamp;\
    } while (0);
void copyMeta(Meta* a, Meta* b) {
    a->gid = b->gid;
    a->timestamp = b->timestamp;
}

typedef struct CRDT_LWW_Register {//
    unsigned char type;
    unsigned char gid;
    long long timestamp;//8
    VectorClock vectorClock;//9
} __attribute__ ((packed, aligned(4))) CRDT_LWW_Register;
typedef struct Crdt_Final_Object {//
    unsigned char type;
} __attribute__ ((packed, aligned(2))) Crdt_Final_Object;

#define CRDT_DATA 1
#define CRDT_EXPIRE 2
#define CRDT_EXPIRE 3
#define CRDT_EXPIRE_TOMBSTONE 4
#define CRDT_REGISTER 1
#define CRDT_HASH 2
// 0 0 0 0  |  0 0 0 0
int isExpire(Crdt_Final_Object* obj) {
    if(obj->type & CRDT_EXPIRE) {
        return 1;
    }
    return 0;
}
int isData(Crdt_Final_Object* obj) {
    if(obj->type & CRDT_DATA) {
        return 1;
    }
    return 0;
}
int getDataType(Crdt_Final_Object* obj) {
    return obj->type & 0x0F;
}
int setDataType(Crdt_Final_Object* obj, int type) {
    obj->type &= 0xF0;
    obj->type |= type;
}
void setType(Crdt_Final_Object* obj, int type) {
    obj->type &= 0x0F;
    obj->type |= (type << 4);
}
int getType(Crdt_Final_Object* obj) {
    return obj->type >> 4;
}

int getGid(void* obj) {
    int gid = 0;
    gid += (int)obj;
    return gid;
}
typedef struct Crdt_Final_Register {//
    unsigned char type;
    unsigned char gid;
    long long timestamp;//8

} __attribute__ ((packed, aligned(4))) Crdt_Final_Register;
int main() {
    printf("Object sizeof  %lu  \n", sizeof(Crdt_Final_Register));
    Crdt_Final_Register *r =  malloc(sizeof(Crdt_Final_Register));
    setType(r, CRDT_DATA | CRDT_EXPIRE);
    setType(r, CRDT_DATA );
    assert(getType(r) == CRDT_DATA);
    setDataType(r, CRDT_HASH | CRDT_REGISTER);
    setDataType(r, CRDT_REGISTER);
    assert(getDataType(r) == CRDT_REGISTER);
    Meta meta;
    meta.gid = 1000;
    meta.timestamp = 1000;
    size_t size = zmalloc_used_memory();
    Meta* m = zmalloc(sizeof(Crdt_Final_Register));
    printf("size1: %lld sizeof: %lld \n", zmalloc_used_memory() - size, sizeof(Crdt_Final_Register));
    m->gid = 1000;
    m->timestamp = 2000; 
    Meta k = *m;
    Crdt_Final_Register *other =  zmalloc(sizeof(Crdt_Final_Register));
    other->gid = 4;
    other->timestamp = 3000;
    copyMeta2(r, m);
    printf("is data  %d  \n", isData(r));
    printf("gid  %d \n", (r->gid));
    printf("gid  %lld \n", (r->timestamp));
    printf("mem_allocator: %s \n",ZMALLOC_LIB);
    zfree(m);
    zfree(other);
    return 1;
}