
#ifndef __LATTE_OBJECT_H
#define __LATTE_OBJECT_H
#include <limits.h>
#include <inttypes.h>
#include <ctype.h>

/* The actual Redis Object */
#define OBJ_STRING 0    /* String object. */
#define OBJ_LIST 1      /* List object. */
#define OBJ_SET 2       /* Set object. */
#define OBJ_ZSET 3      /* Sorted set object. */
#define OBJ_HASH 4      /* Hash object. */
#define OBJ_MODULE 5    /* Module object. */
#define OBJ_STREAM 6    /* Stream object. */

#define LRU_BITS 24
#define LRU_CLOCK_MAX ((1<<LRU_BITS)-1) /* Max value of obj->lru */

/* Objects encoding. Some kind of objects like Strings and Hashes can be
 * internally represented in multiple ways. The 'encoding' field of the object
 * is set to one of this fields for this object. */
#define OBJ_ENCODING_RAW 0     /* Raw representation */
#define OBJ_ENCODING_INT 1     /* Encoded as integer */
#define OBJ_ENCODING_HT 2      /* Encoded as hash table */
#define OBJ_ENCODING_ZIPMAP 3  /* Encoded as zipmap */
#define OBJ_ENCODING_LINKEDLIST 4 /* No longer used: old list encoding. */
#define OBJ_ENCODING_ZIPLIST 5 /* Encoded as ziplist */
#define OBJ_ENCODING_INTSET 6  /* Encoded as intset */
#define OBJ_ENCODING_SKIPLIST 7  /* Encoded as skiplist */
#define OBJ_ENCODING_EMBSTR 8  /* Embedded sds string encoding */
#define OBJ_ENCODING_QUICKLIST 9 /* Encoded as linked list of ziplists */
#define OBJ_ENCODING_STREAM 10 /* Encoded as a radix tree of listpacks */
#define OBJ_ENCODING_LISTPACK 11 /* Encoded as a listpack */
// #define OBJ_ENCODING_LISTPACK_EX 12 /* Encoded as listpack, extended with metadata */

#define OBJ_SHARED_REFCOUNT INT_MAX     /* Global object never destroyed. */
#define OBJ_STATIC_REFCOUNT (INT_MAX-1) /* Object allocated in the stack. */
#define OBJ_FIRST_SPECIAL_REFCOUNT OBJ_STATIC_REFCOUNT


typedef struct latte_object_t {
    unsigned type:4;
    unsigned encoding:4;
    unsigned lru:LRU_BITS;
    int refcount; /* LRU 时间（相对于全局 lru_clock）或
                * LFU 数据（最低有效 8 位频率
                * 和最高有效 16 位访问时间）。 */
    void *ptr;
} latte_object_t;
#define sds_encoded_object(objptr) (objptr->encoding == OBJ_ENCODING_RAW || objptr->encoding == OBJ_ENCODING_EMBSTR)

latte_object_t* latte_object_new(int type, void* ptr);
void latte_object_decr_ref_count(latte_object_t* o);
void latte_object_incr_ref_count(latte_object_t* o);
latte_object_t* make_object_shared(latte_object_t* o);

char* str_encoding(int encoding);
size_t object_string_compute_size(latte_object_t* o, size_t sample_size);
size_t object_list_compute_size(latte_object_t* o, size_t sample_size);
size_t object_set_compute_size(latte_object_t* o, size_t sample_size);
size_t object_zset_compute_size(latte_object_t* o, size_t sample_size);
size_t object_hash_compute_size(latte_object_t* o, size_t sample_size);
size_t object_stream_compute_size(latte_object_t* o, size_t sample_size);
size_t object_module_compute_size(latte_object_t *key, latte_object_t* o, size_t sample_size, int dbid);
size_t object_compute_size(latte_object_t *key, latte_object_t* o, size_t sample_size, int dbid);


void free_string_object(latte_object_t *o);
void free_list_object(latte_object_t *o);
void free_set_object(latte_object_t *o);
void free_zset_object(latte_object_t* o);
void free_hash_object(latte_object_t* o);
void free_module_object(latte_object_t *o);
void free_stream_object(latte_object_t *o);
#endif