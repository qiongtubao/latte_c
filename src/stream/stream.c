

#include "stream.h"
#include "zmalloc/zmalloc.h"
#include "rax/rax.h"
#include "list/listpack.h"
stream_t* stream_new() {
    stream_t *s = zmalloc(sizeof(*s));
    s->rax = raxNew();
    s->length = 0;
    s->first_id.ms = 0;
    s->first_id.seq = 0;
    s->last_id.ms = 0;
    s->last_id.seq = 0;
    s->max_deleted_entry_id.seq = 0;
    s->max_deleted_entry_id.ms = 0;
    s->entries_added = 0;
    s->cgroups = NULL; /* Created on demand to save memory when not used. */
    return s;
}

/* Free a NACK entry. */
void stream_free_nack(stream_nack_t *na) {
    zfree(na);
}

/* Free a consumer and associated data structures. Note that this function
 * will not reassign the pending messages associated with this consumer
 * nor will delete them from the stream, so when this function is called
 * to delete a consumer, and not when the whole stream is destroyed, the caller
 * should do some work before. */
void stream_free_consumer(stream_consumer_t *sc) {
    raxFree(sc->pel); /* No value free callback: the PEL entries are shared
                         between the consumer and the main stream PEL. */
    sds_delete(sc->name);
    zfree(sc);
}


/* Free a consumer group and all its associated data. */
void stream_free_cg(stream_cg_t *cg) {
    raxFreeWithCallback(cg->pel,(void(*)(void*))stream_free_nack);
    raxFreeWithCallback(cg->consumers,(void(*)(void*))stream_free_consumer);
    zfree(cg);
}

void stream_delete(stream_t* s) {
    raxFreeWithCallback(s->rax, (void(*)(void*))lp_free);
    if (s->cgroups) 
        raxFreeWithCallback(s->cgroups, (void(*)(void*))stream_free_cg);
    zfree(s);
}