#ifndef __LATTE_CUCKOO_H
#define __LATTE_CUCKOO_H

#include <stdint.h>
#include <stddef.h>

#define CUCKOO_OK 0
#define CUCKOO_ERR -1

#define CUCKOO_UNUSED(V) ((void) V)

#define CUCKOO_FILTER_MAX_ITERATION 500
#define CUCKOO_FILTER_TAGS_PER_BUCKET  4
#define CUCKOO_FILTER_BUCKETS_EXPANSION 4
#define CUCKOO_FILTER_MAX_TABLES 4
#define CUCKOO_TAG_NULL 0
#define CUCKOO_FILTER_TABLE_MIN_BUCKETS  16

#define CUCKOO_FILTER_BITS_PER_TAG_8  0
#define CUCKOO_FILTER_BITS_PER_TAG_12 1
#define CUCKOO_FILTER_BITS_PER_TAG_16 2
#define CUCKOO_FILTER_BITS_PER_TAG_32 3
#define CUCKOO_FILTER_BITS_PER_TAG_TYPES 4


typedef uint64_t (*cuckoo_hash_fn)(const void *key, int klen);

typedef struct cuckoo_victim_cache_t {
    int used;
    uint32_t tag;
    size_t index;
} cuckoo_victim_cache_t;

typedef struct cuckoo_table_t {
    size_t bits_per_tag;
    size_t bytes_per_bucket;
    size_t nbuckets;
    cuckoo_victim_cache_t victim;
    size_t ntags;
    uint8_t *data;
} cuckoo_table_t;

/* Cuckoo filter consists of cuckoo table with N, 4N, 16N... buckets. */
typedef struct cuckoo_filter_t {
    cuckoo_hash_fn hash_fn;
    int bits_per_tag;
    int ntables;
    cuckoo_table_t *tables;
} cuckoo_filter_t;

typedef struct cuckoo_filter_stat_t {
  size_t ntags;
  size_t used_memory;
  size_t ntables;
  double load_factor;
  double load_factors[CUCKOO_FILTER_MAX_TABLES];
} cuckoo_filter_stat_t;

uint64_t cuckoo_gen_hash_function(const void *key, int len);

cuckoo_filter_t* cuckoo_filter_new(cuckoo_hash_fn hash_fn, int bits_per_tag_type, size_t estimated_keys);
void cuckoo_filter_free(cuckoo_filter_t* cuckoo);

int cuckoo_filter_insert(cuckoo_filter_t* filter, const char *key, size_t klen);

int cuckoo_filter_contains(cuckoo_filter_t* filter, const char *key, size_t klen);

int cuckoo_filter_delete(cuckoo_filter_t* filter, const char *key, size_t klen);

void cuckoo_filter_get_stat(cuckoo_filter_t* filter, cuckoo_filter_stat_t* stat);

size_t cuckoo_filter_used_memory(cuckoo_filter_t* filter);

int is_pow_of_2(uint64_t n);
uint64_t upper_pow_of_2(uint64_t n);
#endif