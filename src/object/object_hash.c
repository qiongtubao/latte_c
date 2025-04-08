

#include "hash.h"
#include "list/ziplist.h"

#include "list/listpack.h"
#include "debug/latte_debug.h"

latte_object_t* hash_object_new(dict_t* d) {
    latte_object_t* o = latte_object_new(OBJ_HASH, d);
    o->encoding = OBJ_ENCODING_HT;
    return o;
}
//被淘汰
latte_object_t* ziplist_hash_object_new() {
    unsigned char *zl = zip_list_new();
    latte_object_t* o = latte_object_new(OBJ_HASH, zl);
    o->encoding = OBJ_ENCODING_ZIPLIST;
    return o;
}

latte_object_t* listpack_hash_object_new() {
    unsigned char* lp = lp_new(0);
    latte_object_t* o = latte_object_new(OBJ_HASH, lp);
    o->encoding = OBJ_ENCODING_LISTPACK;
    return o;
}




void free_hash_object(latte_object_t* o) {
    switch (o->encoding) {
        case OBJ_ENCODING_HT:
            /* Verify hash is not registered in global HFE ds */
            // 7.x支持 subkey expire  新结构
            // if (isDictWithMetaHFE((dict_t*)o->ptr)) {
            //     dict_expire_metadata_t *m = (dict_expire_metadata_t *)dictMetadata((dict_t*)o->ptr);
            //     serverAssert(m->expireMeta.trash == 1);
            // }
            dict_delete((dict_t*) o->ptr);
            break;
        case OBJ_ENCODING_ZIPLIST:
            zfree(o->ptr);
            break;
        case OBJ_ENCODING_LISTPACK:
            lp_free(o->ptr);
            break;
        // case OBJ_ENCODING_LISTPACK_EX:  //7.x  dict过期
        //     /* Verify hash is not registered in global HFE ds */
        //     latte_assert(((list_pack_ex_t *) o->ptr)->meta.trash == 1);
        //     list_pack_ex_free(o->ptr);
        //     break;
        default:
            latte_panic("Unknown hash encoding type");
            break;
    }
}

size_t object_hash_compute_size(latte_object_t* o, size_t sample_size) {
    dict_t* d;
    latte_iterator_t* di;
    size_t asize = 0, elesize = 0, samples = 0;
    dict_entry_t *de;
    sds ele, ele2;
    if (o->encoding == OBJ_ENCODING_LISTPACK) {
        asize = sizeof(*o)+zmalloc_size(o->ptr);
    } else if (o->encoding == OBJ_ENCODING_ZIPLIST) {
        asize = sizeof(*o)+(zmalloc_size(o->ptr));
    } else if (o->encoding == OBJ_ENCODING_HT) {
        d = o->ptr;
        di = dict_get_latte_iterator(d);
        asize = sizeof(*o)+sizeof(dict_t)+(sizeof(struct dict_entry_t*)*dict_slots(d));
        while(latte_iterator_has_next(di) && samples < sample_size) {
            de = latte_iterator_next(di);
            ele = dict_get_entry_key(de);
            ele2 = dict_get_entry_val(de);
            elesize += sds_zmalloc_size(ele) + sds_zmalloc_size(ele2);
            elesize += sizeof(struct dict_entry_t);
            samples++;
        }
        latte_iterator_delete(di);
        if (samples) asize += (double)elesize/samples*dict_size(d);
    } else {
        latte_panic("Unknown hash encoding");
    }
    return asize;
}