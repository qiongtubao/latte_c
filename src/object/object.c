#include "object.h"
#include "sds/sds.h"
#include "dict/dict.h"
#include "log/log.h"
#include "debug/latte_debug.h"

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
        switch(o->type) {
            case OBJ_STRING: free_string_object(o); break;
            case OBJ_LIST: free_list_object(o); break;
            case OBJ_SET: free_set_object(o); break;
            case OBJ_ZSET: free_zset_object(o); break;     
            case OBJ_HASH: free_hash_object(o); break;
            // case OBJ_MODULE: freeModuleObject(o); break;
            // case OBJ_STREAM: freeStreamObject(o); break;
            default: latte_panic("Unknown object type"); break;
        }
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


size_t object_compute_size(latte_object_t *key, latte_object_t* o, size_t sample_size, int dbid) {
    sds ele, ele2;
    dict_t *d;
    latte_iterator_t *di;
    // struct dict_entry_t *de;
    size_t asize = 0, elesize = 0, samples = 0;

    if (o->type == OBJ_STRING) {
        asize =  object_string_compute_size(o, sample_size);
    } else if (o->type == OBJ_LIST) {
        asize = object_list_compute_size(o, sample_size);
        // if (o->encoding == OBJ_ENCODING_QUICKLIST) {
        //     quick_list_t *ql = o->ptr;
        //     quick_list_node_t *node = ql->head;
        //     asize = sizeof(*o)+sizeof(quick_list_t);
        //     do {
        //         elesize += sizeof(quick_list_node_t)+ziplistBlobLen(node->zl);
        //         samples++;
        //     } while ((node = node->next) && samples < sample_size);
        //     asize += (double)elesize/samples*ql->len;
        // } else if (o->encoding == OBJ_ENCODING_ZIPLIST) {
        //     asize = sizeof(*o)+ziplistBlobLen(o->ptr);
        // } else {
        //     serverPanic("Unknown list encoding");
        // }
    } else if (o->type == OBJ_SET) {
        asize = object_set_compute_size(o, sample_size);
        // if (o->encoding == OBJ_ENCODING_HT) {
        //     d = o->ptr;
        //     di = dict_get_latte_iterator(d);
        //     asize = sizeof(*o)+sizeof(dict_t)+(sizeof(struct dict_entry_t*)*dictSlots(d));
        //     while((de = dictNext(di)) != NULL && samples < sample_size) {
        //         ele = dictGetKey(de);
        //         elesize += sizeof(struct dict_entry_t) + sdsZmallocSize(ele);
        //         samples++;
        //     }
        //     dictReleaseIterator(di);
        //     if (samples) asize += (double)elesize/samples*dictSize(d);
        // } else if (o->encoding == OBJ_ENCODING_INTSET) {
        //     int_set_t *is = o->ptr;
        //     asize = sizeof(*o)+sizeof(*is)+(size_t)is->encoding*is->length;
        // } else {
        //     // serverPanic("Unknown set encoding");
        //     exit(0);
        // }
    } else if (o->type == OBJ_ZSET) {
        asize = object_zset_compute_size(o, sample_size);
        // if (o->encoding == OBJ_ENCODING_ZIPLIST) {
        //     asize = sizeof(*o)+(ziplistBlobLen(o->ptr));
        // } else if (o->encoding == OBJ_ENCODING_SKIPLIST) {
        //     d = ((zset_t*)o->ptr)->dict;
        //     zskiplist *zsl = ((zset*)o->ptr)->zsl;
        //     zskiplistNode *znode = zsl->header->level[0].forward;
        //     asize = sizeof(*o)+sizeof(zset)+sizeof(zskiplist)+sizeof(dict_t)+
        //             (sizeof(struct dict_entry_t*)*dictSlots(d))+
        //             zmalloc_size(zsl->header);
        //     while(znode != NULL && samples < sample_size) {
        //         elesize += sdsZmallocSize(znode->ele);
        //         elesize += sizeof(struct dict_entry_t) + zmalloc_size(znode);
        //         samples++;
        //         znode = znode->level[0].forward;
        //     }
        //     if (samples) asize += (double)elesize/samples*dictSize(d);
        // } else {
        //     serverPanic("Unknown sorted set encoding");
        // }
    } else if (o->type == OBJ_HASH) {
        asize = object_hash_compute_size(o, sample_size);
        // if (o->encoding == OBJ_ENCODING_ZIPLIST) {
        //     asize = sizeof(*o)+(ziplistBlobLen(o->ptr));
        // } else if (o->encoding == OBJ_ENCODING_HT) {
        //     d = o->ptr;
        //     di = dictGetIterator(d);
        //     asize = sizeof(*o)+sizeof(dict_t)+(sizeof(struct dict_entry_t*)*dictSlots(d));
        //     while((de = dictNext(di)) != NULL && samples < sample_size) {
        //         ele = dictGetKey(de);
        //         ele2 = dictGetVal(de);
        //         elesize += sdsZmallocSize(ele) + sdsZmallocSize(ele2);
        //         elesize += sizeof(struct dict_entry_t);
        //         samples++;
        //     }
        //     dictReleaseIterator(di);
        //     if (samples) asize += (double)elesize/samples*dictSize(d);
        // } else {
        //     serverPanic("Unknown hash encoding");
        // }
    } else if (o->type == OBJ_STREAM) {
        asize = object_stream_compute_size(o, sample_size);
        // stream *s = o->ptr;
        // asize = sizeof(*o)+sizeof(*s);
        // asize += streamRadixTreeMemoryUsage(s->rax);

        // /* Now we have to add the listpacks. The last listpack is often non
        //  * complete, so we estimate the size of the first N listpacks, and
        //  * use the average to compute the size of the first N-1 listpacks, and
        //  * finally add the real size of the last node. */
        // raxIterator ri;
        // raxStart(&ri,s->rax);
        // raxSeek(&ri,"^",NULL,0);
        // size_t lpsize = 0, samples = 0;
        // while(samples < sample_size && raxNext(&ri)) {
        //     unsigned char *lp = ri.data;
        //     /* Use the allocated size, since we overprovision the node initially. */
        //     lpsize += zmalloc_size(lp);
        //     samples++;
        // }
        // if (s->rax->numele <= samples) {
        //     asize += lpsize;
        // } else {
        //     if (samples) lpsize /= samples; /* Compute the average. */
        //     asize += lpsize * (s->rax->numele-1);
        //     /* No need to check if seek succeeded, we enter this branch only
        //      * if there are a few elements in the radix tree. */
        //     raxSeek(&ri,"$",NULL,0);
        //     raxNext(&ri);
        //     /* Use the allocated size, since we overprovision the node initially. */
        //     asize += zmalloc_size(ri.data);
        // }
        // raxStop(&ri);

        // /* Consumer groups also have a non trivial memory overhead if there
        //  * are many consumers and many groups, let's count at least the
        //  * overhead of the pending entries in the groups and consumers
        //  * PELs. */
        // if (s->cgroups) {
        //     raxStart(&ri,s->cgroups);
        //     raxSeek(&ri,"^",NULL,0);
        //     while(raxNext(&ri)) {
        //         streamCG *cg = ri.data;
        //         asize += sizeof(*cg);
        //         asize += streamRadixTreeMemoryUsage(cg->pel);
        //         asize += sizeof(streamNACK)*raxSize(cg->pel);

        //         /* For each consumer we also need to add the basic data
        //          * structures and the PEL memory usage. */
        //         raxIterator cri;
        //         raxStart(&cri,cg->consumers);
        //         raxSeek(&cri,"^",NULL,0);
        //         while(raxNext(&cri)) {
        //             streamConsumer *consumer = cri.data;
        //             asize += sizeof(*consumer);
        //             asize += sdslen(consumer->name);
        //             asize += streamRadixTreeMemoryUsage(consumer->pel);
        //             /* Don't count NACKs again, they are shared with the
        //              * consumer group PEL. */
        //         }
        //         raxStop(&cri);
        //     }
        //     raxStop(&ri);
        // }
    } else if (o->type == OBJ_MODULE) {
        asize = object_module_compute_size(key, o, sample_size, dbid);
        // moduleValue *mv = o->ptr;
        // moduleType *mt = mv->type;
        // if (mt->mem_usage != NULL) {
        //     asize = mt->mem_usage(mv->value);
        // } else {
        //     asize = 0;
        // }
    } else {
        latte_panic("Unknown object type");
    }
    return asize;
}