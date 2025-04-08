

#include "stream.h"
#include "stream/stream.h"
#include "zmalloc/zmalloc.h"
#include "rax/rax.h"

latte_object_t *stream_object_new() {
    stream_t *s = stream_new();
    latte_object_t *o = latte_object_new(OBJ_STREAM,s);
    o->encoding = OBJ_ENCODING_STREAM;
    return o;
}

void free_stream_object(latte_object_t *o) {
    stream_delete(o->ptr);
}


size_t streamRadixTreeMemoryUsage(rax *rax) {
    size_t size = sizeof(*rax);
    size = rax->numele * sizeof(stream_id_t);
    size += rax->numnodes * sizeof(raxNode);
    /* Add a fixed overhead due to the aux data pointer, children, ... */
    size += rax->numnodes * sizeof(long)*30;
    return size;
}

size_t object_stream_compute_size(latte_object_t* o, size_t sample_size) {
    stream_t *s = o->ptr;
    size_t asize = sizeof(*o)+sizeof(*s);
    asize += streamRadixTreeMemoryUsage(s->rax);

    /* Now we have to add the listpacks. The last listpack is often non
     * complete, so we estimate the size of the first N listpacks, and
     * use the average to compute the size of the first N-1 listpacks, and
     * finally add the real size of the last node. */
    raxIterator ri;
    raxStart(&ri,s->rax);
    raxSeek(&ri,"^",NULL,0);
    size_t lpsize = 0, samples = 0;
    while(samples < sample_size && raxNext(&ri)) {
        unsigned char *lp = ri.data;
        /* Use the allocated size, since we overprovision the node initially. */
        lpsize += zmalloc_size(lp);
        samples++;
    }
    if (s->rax->numele <= samples) {
        asize += lpsize;
    } else {
        if (samples) lpsize /= samples; /* Compute the average. */
        asize += lpsize * (s->rax->numele-1);
        /* No need to check if seek succeeded, we enter this branch only
         * if there are a few elements in the radix tree. */
        raxSeek(&ri,"$",NULL,0);
        raxNext(&ri);
        /* Use the allocated size, since we overprovision the node initially. */
        asize += zmalloc_size(ri.data);
    }
    raxStop(&ri);

    /* Consumer groups also have a non trivial memory overhead if there
     * are many consumers and many groups, let's count at least the
     * overhead of the pending entries in the groups and consumers
     * PELs. */
    if (s->cgroups) {
        raxStart(&ri,s->cgroups);
        raxSeek(&ri,"^",NULL,0);
        while(raxNext(&ri)) {
            stream_cg_t *cg = ri.data;
            asize += sizeof(*cg);
            asize += streamRadixTreeMemoryUsage(cg->pel);
            asize += sizeof(stream_nack_t)*raxSize(cg->pel);

            /* For each consumer we also need to add the basic data
             * structures and the PEL memory usage. */
            raxIterator cri;
            raxStart(&cri,cg->consumers);
            raxSeek(&cri,"^",NULL,0);
            while(raxNext(&cri)) {
                stream_consumer_t *consumer = cri.data;
                asize += sizeof(*consumer);
                asize += sds_len(consumer->name);
                asize += streamRadixTreeMemoryUsage(consumer->pel);
                /* Don't count NACKs again, they are shared with the
                 * consumer group PEL. */
            }
            raxStop(&cri);
        }
        raxStop(&ri);
    }
}