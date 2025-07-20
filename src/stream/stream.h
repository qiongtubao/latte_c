

#ifndef __LATTE_STREAM_H
#define __LATTE_STREAM_H

#include <stdio.h>
#include <stdlib.h>
#include <rax/rax.h>
#include "sds/sds.h"

/* Stream item ID: a 128 bit number composed of a milliseconds time and
 * a sequence counter. IDs generated in the same millisecond (or in a past
 * millisecond if the clock jumped backward) will use the millisecond time
 * of the latest generated ID and an incremented sequence. */
typedef struct stream_id_t {
    uint64_t ms;        /* Unix time in milliseconds. */
    uint64_t seq;       /* Sequence number. */
} stream_id_t;
typedef struct stream_t {
    rax *rax;               /* The radix tree holding the stream. */
    uint64_t length;        /* Current number of elements inside this stream. */
    stream_id_t last_id;       /* Zero if there are yet no items. */
    stream_id_t first_id;      /* The first non-tombstone entry, zero if empty. */
    stream_id_t max_deleted_entry_id;  /* The maximal ID that was deleted. */
    uint64_t entries_added; /* All time count of elements added. */
    rax *cgroups;           /* Consumer groups dictionary: name -> streamCG */
} stream_t;

stream_t* stream_new();
void stream_delete(stream_t*);
typedef struct stream_cg_t {
    stream_id_t last_id;    /* Last delivered (not acknowledged) ID for this
                               group. Consumers that will just ask for more
                               messages will served with IDs > than this. */
    long long entries_read; /* In a perfect world (CG starts at 0-0, no dels, no
                               XGROUP SETID, ...), this is the total number of
                               group reads. In the real world, the reasoning behind
                               this value is detailed at the top comment of
                               streamEstimateDistanceFromFirstEverEntry(). */
    rax *pel;               /* Pending entries list. This is a radix tree that
                               has every message delivered to consumers (without
                               the NOACK option) that was yet not acknowledged
                               as processed. The key of the radix tree is the
                               ID as a 64 bit big endian number, while the
                               associated value is a streamNACK structure.*/
    rax *consumers;         /* A radix tree representing the consumers by name
                               and their associated representation in the form
                               of streamConsumer structures. */

} stream_cg_t;

typedef long long mstime_t; /* millisecond time type. */
typedef long long ustime_t; /* microsecond time type. */
typedef struct stream_consumer_t {
    mstime_t seen_time;         /* Last time this consumer tried to perform an action (attempted reading/claiming). */
    mstime_t active_time;       /* Last time this consumer was active (successful reading/claiming). */
    sds name;                   /* Consumer name. This is how the consumer
                                   will be identified in the consumer group
                                   protocol. Case sensitive. */
    rax *pel;                   /* Consumer specific pending entries list: all
                                   the pending messages delivered to this
                                   consumer not yet acknowledged. Keys are
                                   big endian message IDs, while values are
                                   the same streamNACK structure referenced
                                   in the "pel" of the consumer group structure
                                   itself, so the value is shared. */

} stream_consumer_t;

typedef struct stream_nack_t {
    mstime_t delivery_time;     /* Last time this message was delivered. */
    uint64_t delivery_count;    /* Number of times this message was delivered.*/
    stream_consumer_t *consumer;   /* The consumer this message was delivered to
                                   in the last delivery. */
} stream_nack_t;
#endif