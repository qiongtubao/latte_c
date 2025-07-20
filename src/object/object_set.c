

#include "set.h"
#include "dict/dict.h"
#include "dict/dict_plugins.h"
#include "set/int_set.h"
#include "debug/latte_debug.h"
#include "sds/sds.h"

//以后再修改成hash_set模型把  现在暂时用hash
latte_object_t* hash_set_object_new(dict_t* d) {
    // dict_t* d =  dict_new(&sds_key_set_dict_type);   
    latte_object_t* o = latte_object_new(OBJ_SET,d);
    o->encoding = OBJ_ENCODING_HT;
    return o;
}

latte_object_t* intset_object_new() {
    int_set_t* is = int_set_new();
    latte_object_t* o = latte_object_new(OBJ_SET,is);
    o->encoding = OBJ_ENCODING_INTSET;
    return o;
}

void free_set_object(latte_object_t* o) {
    switch (o->encoding) {
        case OBJ_ENCODING_HT:
            dict_delete((dict_t*) o->ptr);
            break;
        case OBJ_ENCODING_INTSET:
        case OBJ_ENCODING_LISTPACK:
            zfree(o->ptr);
            break;
        default:
            latte_panic("Unknown set encoding type");
    }
}

size_t object_set_compute_size(latte_object_t* o, size_t sample_size) {
    size_t asize = 0, samples = 0, elesize = 0;
    dict_t* d;
    latte_iterator_t* di;
    dict_entry_t* de;
    sds ele;
    if (o->encoding == OBJ_ENCODING_HT) {
        d = o->ptr;
        di = dict_get_latte_iterator(d);
        asize = sizeof(*o)+sizeof(dict_t)+(sizeof(struct dict_entry_t*)*dict_slots(d));
        while(latte_iterator_has_next(di) && samples < sample_size) {
            de = latte_iterator_next(di);
            ele = dict_get_entry_key(de);
            elesize += sizeof(struct dict_entry_t) + sds_zmalloc_size(ele);
            samples++;
        }
        latte_iterator_delete(di);
        if (samples) asize += (double)elesize/samples*dict_size(d);
    } else if (o->encoding == OBJ_ENCODING_INTSET) {
        int_set_t *is = o->ptr;
        asize = sizeof(*o)+sizeof(*is)+(size_t)is->encoding*is->length;
    } else {
        latte_panic("Unknown set encoding");
    }
    return asize;
}