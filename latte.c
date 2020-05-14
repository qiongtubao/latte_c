#include "assert.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "zmalloc.h"
#include "latte.h"
typedef struct VectorClock {
    union {
        long long single;
        void *multi;
    };
} __attribute__ ((packed, aligned(4))) VectorClock;
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
typedef long long  VC;
typedef struct CRDT_LWW_Register {//
    // unsigned long long type:8;
    // unsigned long long gid:4;
    // unsigned long long timestamp:48;
    int vc;
    // VectorClock vc;
} __attribute__ ((packed, aligned(1))) CRDT_LWW_Register;
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
    unsigned char a[7];
    long long timestamp;//8
} __attribute__ ((packed, aligned(4))) Crdt_Final_Register;
int main(int argc, char **argv) {
    size_t size = zmalloc_used_memory();
    Crdt_Final_Register *r =  zmalloc(sizeof(Crdt_Final_Register));
    printf("mem_allocator: %s\n CRDT_LWW_Register sizeof  %lu %lu \n",ZMALLOC_LIB, sizeof(Crdt_Final_Register), zmalloc_used_memory() - size);
    return 1;
}