

#include "zset.h"
#include "zset/zset.h"
#include "list/listpack.h";
#include "debug/latte_debug.h"
#include "list/ziplist.h"
latte_object_t* zset_object_new(dict_t* d) {
    zset_t* zs = zset_new(d);
    latte_object_t* o = latte_object_new(OBJ_ZSET, zs);
    o->encoding = OBJ_ENCODING_SKIPLIST;
    return o;
}

//被废弃
latte_object_t* zset_zip_list_object_new() {
    unsigned char* zl = zip_list_new();
    latte_object_t* o = latte_object_new(OBJ_ZSET, zl);
    o->encoding = OBJ_ENCODING_ZIPLIST;
    return o;
}

latte_object_t* zset_list_pack_object_new() {
    unsigned char* lp = lp_new(0);
    latte_object_t* o = latte_object_new(OBJ_ZSET, lp);
    o->encoding = OBJ_ENCODING_LISTPACK;
    return o;
}


void free_zset_object(latte_object_t* o) {
    zset_t *zs;
    switch (o->encoding) {
    case OBJ_ENCODING_SKIPLIST:
        zs = o->ptr;
        zset_delete(zs);
        break;
    case OBJ_ENCODING_LISTPACK:
        zfree(o->ptr);
        break;
    default:
        latte_panic("Unknown sorted set encoding");
    }
}

size_t object_zset_compute_size(latte_object_t* o, size_t sample_size) {
    size_t asize = 0, elesize = 0, samples = 0;
    dict_t* d;
    if (o->encoding == OBJ_ENCODING_LISTPACK) {
        asize = sizeof(*o)+zmalloc_size(o->ptr);
    } else if (o->encoding == OBJ_ENCODING_ZIPLIST) {
        asize = sizeof(*o)+(zmalloc_size(o->ptr));
    } else if (o->encoding == OBJ_ENCODING_SKIPLIST) {
        d = ((zset_t*)o->ptr)->dict;
        zskip_list_t *zsl = ((zset_t*)o->ptr)->zsl;
        zskip_list_node_t *znode = zsl->header->level[0].forward;
        asize = sizeof(*o)+sizeof(zset_t)+sizeof(zskip_list_t)+sizeof(dict_t)+
                (sizeof(struct dict_entry_t*)*dict_slots(d))+
                zmalloc_size(zsl->header);
        while(znode != NULL && samples < sample_size) {
            elesize += sds_zmalloc_size(znode->ele);
            elesize += sizeof(struct dict_entry_t) + zmalloc_size(znode);
            samples++;
            znode = znode->level[0].forward;
        }
        if (samples) asize += (double)elesize/samples*dict_size(d);
    } else {
        latte_panic("Unknown sorted set encoding");
    }
    return asize;
}