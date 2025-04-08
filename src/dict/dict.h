
#ifndef __LATTE_DICT_H
#define __LATTE_DICT_H

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include "iterator/iterator.h"
#include "cmp/cmp.h"

#define DICT_OK 0
#define DICT_ERR 1

/* Unused arguments generate annoying warnings... */
#define DICT_NOTUSED(V) ((void) V)


typedef struct dict_entry_t {
    void *key;
    union {
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;
    struct dict_entry_t*next;     /* Next entry in the same hash bucket. */
    void *metadata[];           /* An arbitrary number of bytes (starting at a
                                 * pointer-aligned address) of size as returned
                                 * by dict_func_t's dictEntryMetadataBytes(). */
} dict_entry_t;

typedef struct dict_t dict_t;

typedef struct dict_func_t {
    uint64_t (*hashFunction)(const void *key);
    void *(*keyDup)(dict_t *d, const void *key);
    void *(*valDup)(dict_t *d, const void *obj);
    int (*keyCompare)(dict_t *d, const void *key1, const void *key2);
    void (*keyDestructor)(dict_t *d, void *key);
    void (*valDestructor)(dict_t *d, void *obj);
    int (*expandAllowed)(size_t moreMem, double usedRatio);
    /* Allow a dict_entry_tto carry extra caller-defined metadata.  The
     * extra memory is initialized to 0 when a dict_entry_tis allocated. */
    size_t (*dictEntryMetadataBytes)(dict_t *d);
} dict_func_t;

#define DICTHT_SIZE(exp) ((exp) == -1 ? 0 : (unsigned long)1<<(exp))
#define DICTHT_SIZE_MASK(exp) ((exp) == -1 ? 0 : (DICTHT_SIZE(exp))-1)

/* Hash table parameters */
#define HASHTABLE_MIN_FILL        10      /* Minimal hash table fill 10% */
#define dict_slots(d) (DICTHT_SIZE((d)->ht_size_exp[0])+DICTHT_SIZE((d)->ht_size_exp[1]))

/**
 *  可参考redis 7.2.5
 */
struct dict_t {
    dict_func_t *type;

    dict_entry_t **ht_table[2];
    unsigned long ht_used[2];

    long rehashidx; /* rehashing not in progress if rehashidx == -1 */

    /* Keep small vars at end for optimal (minimal) struct padding */
    int16_t pauserehash; /* If >0 rehashing is paused (<0 indicates coding error) */
    signed char ht_size_exp[2]; /* exponent of size. (size = 1<<exp) */

    void* metadata[]; //增加expire 针对单个subkey
};





/* This is the initial size of every hash table */
#define DICT_HT_INITIAL_EXP      2
#define DICT_HT_INITIAL_SIZE     (1<<(DICT_HT_INITIAL_EXP))


#define dict_free_val(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d), (entry)->v.val)


#define dict_free_key(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d), (entry)->key)

#define dict_set_key(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        (entry)->key = (d)->type->keyDup((d), _key_); \
    else \
        (entry)->key = (_key_); \
} while(0)

#define dict_compare_keys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d), key1, key2) : \
        (key1) == (key2))

#define dict_get_meta_data(entry) (&(entry)->metadata)
#define dict_get_meta_data_size(d) ((d)->type->dictEntryMetadataBytes \
                             ? (d)->type->dictEntryMetadataBytes(d) : 0)


#define dict_size(d) ((d)->ht_used[0]+(d)->ht_used[1])
#define dict_is_rehashing(d) ((d)->rehashidx != -1)
#define dict_hash_key(d, key) (d)->type->hashFunction(key)


#define dict_get_val(he) ((he)->v.val)


#define dict_set_val(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        (entry)->v.val = (d)->type->valDup((d), _val_); \
    else \
        (entry)->v.val = (_val_); \
} while(0)

#define dict_pause_rehashing(d) (d)->pauserehash++
#define dict_resume_rehashing(d) (d)->pauserehash--

#define dict_get_entry_key(he) ((he)->key)
#define dict_get_entry_val(he) ((he)->v.val)
/* API */
dict_t* dict_new(dict_func_t *type);
void dict_delete(dict_t*d);

int _dict_init(dict_t* d, dict_func_t *ty);
#define  dict_init  _dict_init
void dict_destroy(dict_t*d);


dict_entry_t*dict_add_raw(dict_t*d, void *key, dict_entry_t**existing);
dict_entry_t*dict_add_or_find(dict_t*d, void *key);
dict_entry_t* dict_add_get(dict_t* d, void* key, void* val);
uint64_t dict_gen_case_hash_function(const unsigned char *buf, size_t len);

dict_entry_t* dict_find(dict_t*d, const void *key);
int dict_add(dict_t*d, void *key, void *val);
int dict_expand(dict_t*d, unsigned long size);


/* dict_tbasic function */
uint64_t dict_gen_hash_function(const void *key, size_t len);

int ht_needs_resize(dict_t*dict);
typedef enum {
    DICT_RESIZE_ENABLE,
    DICT_RESIZE_AVOID,
    DICT_RESIZE_FORBID,
} dict_resize_enable_enum;
int dict_resize(dict_t*d);
int dict_delete_key(dict_t*ht, const void *key);




//实现latte_iterator_t 替代原来的遍历
/* If safe is set to 1 this is a safe iterator, that means, you can call
 * dict_add, dict_find, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only dict_next()
 * should be called while iterating. */
typedef struct dict_iterator_t {
    latte_iterator_pair_t sup;
    // dict_t *d;
    long index;
    int table, safe;
    dict_entry_t *entry, *nextEntry;
    /* unsafe iterator fingerprint for misuse detection. */
    unsigned long long fingerprint;
    int readed;
} dict_iterator_t;

// dict_iterator_t *dict_get_iterator(dict_t*d);
// dict_entry_t*dict_next(dict_iterator_t *iter);
// void dict_iterator_delete(dict_iterator_t *iter);

bool protected_dict_iterator_has_next(latte_iterator_t* it);
void protected_dict_iterator_delete(latte_iterator_t* it);
void* protected_dict_iterator_next(latte_iterator_t* it);
latte_iterator_t* dict_get_latte_iterator(dict_t *d);

/** 
 * 非对称性
 *     比如 private_dict_cmp(a, b) + private_dict_cmp(b, a) != 0 
 */
int private_dict_cmp(dict_t* a, dict_t* b, cmp_func cmp);
#endif
