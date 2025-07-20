
#include "list.h"
#include "list/quicklist.h"
#include "list/ziplist.h"
#include "list/listpack.h"
#include "debug/latte_debug.h"
void free_list_object(latte_object_t* o) {
    if (o->encoding == OBJ_ENCODING_QUICKLIST) {
        quick_list_release(o->ptr);
    } else if (o->encoding == OBJ_ENCODING_ZIPLIST) {
        //被弃用
        zfree(o->ptr);
    } else if (o->encoding == OBJ_ENCODING_LISTPACK) {
        lp_free(o->ptr);
    } else {
        latte_panic("Unknown list encoding type");
    }
}


latte_object_t* quick_list_object_new(int fill, int compress) {
    quick_list_t* l = quick_list_new(fill, compress);
    latte_object_t* o = latte_object_new(OBJ_LIST, l);
    o->encoding = OBJ_ENCODING_QUICKLIST;
    return o;
}

latte_object_t* zip_list_object_new() {
    unsigned char *zl = zip_list_new();
    latte_object_t* o = latte_object_new(OBJ_LIST, zl);
    o->encoding = OBJ_ENCODING_ZIPLIST;
    return o;
}

latte_object_t* list_pack_object_new() {
    unsigned char *lp = lp_new(0);
    latte_object_t* o = latte_object_new(OBJ_LIST, lp);
    o->encoding = OBJ_ENCODING_LISTPACK;
    return o;
}

size_t object_list_compute_size(latte_object_t* o, size_t sample_size) {
    size_t asize = 0, elesize = 0, samples = 0;
    if (o->encoding == OBJ_ENCODING_QUICKLIST) {
        quick_list_t *ql = o->ptr;
        quick_list_node_t *node = ql->head;
        asize = sizeof(*o)+sizeof(quick_list_t);
        do {
            elesize += sizeof(quick_list_node_t)+zmalloc_size(node->entry);
            samples++;
        } while ((node = node->next) && samples < sample_size);
        asize += (double)elesize/samples*ql->len;
    } else  if (o->encoding == OBJ_ENCODING_ZIPLIST) {
        //被废弃
        asize = sizeof(*o) + zmalloc_size(o->ptr);
    } else if (o->encoding == OBJ_ENCODING_LISTPACK) {
        asize = sizeof(*o) + zmalloc_size(o->ptr);
    } else {
        latte_panic("Unknown list encoding");
    }
    return asize;
}