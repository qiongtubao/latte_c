
#include "dict.h"
#include "zmalloc/zmalloc.h"
#include <assert.h>
#include <sys/time.h>
#include <string.h>

/* Using dictEnableResize() / dictDisableResize() we make possible to
 * enable/disable resizing of the hash table as needed. This is very important
 * for Redis, as we use copy-on-write and don't want to move too much memory
 * around when there is a child performing saving operations.
 *
 * Note that even when dict_can_resize is set to 0, not all resizes are
 * prevented: a hash table is still allowed to grow if the ratio between
 * the number of elements and the buckets > dict_force_resize_ratio. */
static int dict_can_resize = 1;
static unsigned int dict_force_resize_ratio = 5;





/* -------------------------- hash functions -------------------------------- */

static uint8_t dict_hash_function_seed[16];

void dict_set_hash_function_seed(uint8_t *seed) {
    memcpy(dict_hash_function_seed,seed,sizeof(dict_hash_function_seed));
}

uint8_t *dict_get_hash_function_seed(void) {
    return dict_hash_function_seed;
}

/* The default hashing function uses SipHash implementation
 * in siphash.c. */

uint64_t siphash(const uint8_t *in, const size_t inlen, const uint8_t *k);
uint64_t siphash_nocase(const uint8_t *in, const size_t inlen, const uint8_t *k);

uint64_t dict_gen_hash_function(const void *key, size_t len) {
    return siphash(key,len,dict_hash_function_seed);
}

uint64_t dict_gen_case_hash_function(const unsigned char *buf, size_t len) {
    return siphash_nocase(buf,len,dict_hash_function_seed);
}





/* Reset hash table parameters already initialized with _dict_init()*/
static void _dict_reset(dict_t*d, int htidx)
{
    d->ht_table[htidx] = NULL;
    d->ht_size_exp[htidx] = -1;
    d->ht_used[htidx] = 0;
}


/* Initialize the hash table */
int _dict_init(dict_t*d, dict_func_t*type)
{
    _dict_reset(d, 0);
    _dict_reset(d, 1);
    d->type = type;
    d->rehashidx = -1;
    d->pauserehash = 0;
    return DICT_OK;
}




/* Create a new hash table */
dict_t*dict_new(dict_func_t*type)
{
    dict_t*d = zmalloc(sizeof(*d));

    _dict_init(d,type);
    return d;
}





/* Destroy an entire dictionary */
int _dict_clear(dict_t*d, int htidx, void(callback)(dict_t*)) {
    unsigned long i;

    /* Free all the elements */
    for (i = 0; i < DICTHT_SIZE(d->ht_size_exp[htidx]) && d->ht_used[htidx] > 0; i++) {
        dict_entry_t*he, *nextHe;

        if (callback && (i & 65535) == 0) callback(d);

        if ((he = d->ht_table[htidx][i]) == NULL) continue;
        while(he) {
            nextHe = he->next;
            dict_free_key(d, he);
            dict_free_val(d, he);
            zfree(he);
            d->ht_used[htidx]--;
            he = nextHe;
        }
    }
    /* Free the table and the allocated cache structure */
    zfree(d->ht_table[htidx]);
    /* Re-initialize the table */
    _dict_reset(d, htidx);
    return DICT_OK; /* never fails */
}


/* Clear & Release the hash table */
void dict_delete(dict_t*d)
{
    dict_destroy(d);
    zfree(d);
}

void dict_destroy(dict_t*d) {
    _dict_clear(d,0,NULL);
    _dict_clear(d,1,NULL);
}

static signed char _dict_next_exp(unsigned long size);
static long _dict_key_index(dict_t*d, const void *key, uint64_t hash, dict_entry_t**existing);




/* Expand or create the hash table,
 * when malloc_failed is non-NULL, it'll avoid panic if malloc fails (in which case it'll be set to 1).
 * Returns DICT_OK if expand was performed, and DICT_ERR if skipped. */
int _dict_expand(dict_t*d, unsigned long size, int* malloc_failed)
{
    if (malloc_failed) *malloc_failed = 0;

    /* the size is invalid if it is smaller than the number of
     * elements already inside the hash table */
    if (dict_is_rehashing(d) || d->ht_used[0] > size)
        return DICT_ERR;

    /* the new hash table */
    dict_entry_t**new_ht_table;
    unsigned long new_ht_used;
    signed char new_ht_size_exp = _dict_next_exp(size);

    /* Detect overflows */
    size_t newsize = 1ul<<new_ht_size_exp;
    if (newsize < size || newsize * sizeof(dict_entry_t*) < newsize)
        return DICT_ERR;

    /* Rehashing to the same table size is not useful. */
    if (new_ht_size_exp == d->ht_size_exp[0]) return DICT_ERR;

    /* Allocate the new hash table and initialize all pointers to NULL */
    if (malloc_failed) {
        new_ht_table = ztrycalloc(newsize*sizeof(dict_entry_t*));
        *malloc_failed = new_ht_table == NULL;
        if (*malloc_failed)
            return DICT_ERR;
    } else
        new_ht_table = zcalloc(newsize*sizeof(dict_entry_t*));

    new_ht_used = 0;

    /* Is this the first initialization? If so it's not really a rehashing
     * we just set the first hash table so that it can accept keys. */
    if (d->ht_table[0] == NULL) {
        d->ht_size_exp[0] = new_ht_size_exp;
        d->ht_used[0] = new_ht_used;
        d->ht_table[0] = new_ht_table;
        return DICT_OK;
    }

    /* Prepare a second hash table for incremental rehashing */
    d->ht_size_exp[1] = new_ht_size_exp;
    d->ht_used[1] = new_ht_used;
    d->ht_table[1] = new_ht_table;
    d->rehashidx = 0;
    return DICT_OK;
}


/* return DICT_ERR if expand was not performed */
int dict_expand(dict_t*d, unsigned long size) {
    return _dict_expand(d, size, NULL);
}


/* Performs N steps of incremental rehashing. Returns 1 if there are still
 * keys to move from the old to the new hash table, otherwise 0 is returned.
 *
 * Note that a rehashing step consists in moving a bucket (that may have more
 * than one key as we use chaining) from the old to the new hash table, however
 * since part of the hash table may be composed of empty spaces, it is not
 * guaranteed that this function will rehash even a single bucket, since it
 * will visit at max N*10 empty buckets in total, otherwise the amount of
 * work it does would be unbound and the function may block for a long time. */
int dict_rehash(dict_t*d, int n) {
    int empty_visits = n*10; /* Max number of empty buckets to visit. */
    if (!dict_is_rehashing(d)) return 0;

    while(n-- && d->ht_used[0] != 0) {
        dict_entry_t*de, *nextde;

        /* Note that rehashidx can't overflow as we are sure there are more
         * elements because ht[0].used != 0 */
        printf("exp %ld > %ld", DICTHT_SIZE(d->ht_size_exp[0]), (unsigned long)d->rehashidx);
        assert(DICTHT_SIZE(d->ht_size_exp[0]) > (unsigned long)d->rehashidx);
        while(d->ht_table[0][d->rehashidx] == NULL) {
            d->rehashidx++;
            if (--empty_visits == 0) return 1;
        }
        de = d->ht_table[0][d->rehashidx];
        /* Move all the keys in this bucket from the old to the new hash HT */
        while(de) {
            uint64_t h;

            nextde = de->next;
            /* Get the index in the new hash table */
            h = dict_hash_key(d, de->key) & DICTHT_SIZE_MASK(d->ht_size_exp[1]);
            de->next = d->ht_table[1][h];
            d->ht_table[1][h] = de;
            d->ht_used[0]--;
            d->ht_used[1]++;
            de = nextde;
        }
        d->ht_table[0][d->rehashidx] = NULL;
        d->rehashidx++;
    }

    /* Check if we already rehashed the whole table... */
    if (d->ht_used[0] == 0) {
        zfree(d->ht_table[0]);
        /* Copy the new ht onto the old one */
        d->ht_table[0] = d->ht_table[1];
        d->ht_used[0] = d->ht_used[1];
        d->ht_size_exp[0] = d->ht_size_exp[1];
        _dict_reset(d, 1);
        d->rehashidx = -1;
        return 0;
    }

    /* More to rehash... */
    return 1;
}


/* This function performs just a step of rehashing, and only if hashing has
 * not been paused for our hash table. When we have iterators in the
 * middle of a rehashing we can't mess with the two hash tables otherwise
 * some elements can be missed or duplicated.
 *
 * This function is called by common lookup or update operations in the
 * dictionary so that the hash table automatically migrates from H1 to H2
 * while it is actively used. */
static void _dict_rehash_step(dict_t*d) {
    if (d->pauserehash == 0) dict_rehash(d,1);
}



/* Low level add or find:
 * This function adds the entry but instead of setting a value returns the
 * dict_entry_tstructure to the user, that will make sure to fill the value
 * field as they wish.
 *
 * This function is also directly exposed to the user API to be called
 * mainly in order to store non-pointers inside the hash value, example:
 *
 * entry = dict_add_raw(dict,mykey,NULL);
 * if (entry != NULL) dictSetSignedIntegerVal(entry,1000);
 *
 * Return values:
 *
 * If key already exists NULL is returned, and "*existing" is populated
 * with the existing entry if existing is not NULL.
 *
 * If key was added, the hash entry is returned to be manipulated by the caller.
 */
dict_entry_t*dict_add_raw(dict_t*d, void *key, dict_entry_t**existing)
{
    long index;
    dict_entry_t*entry;
    int htidx;

    if (dict_is_rehashing(d)) _dict_rehash_step(d);

    /* Get the index of the new element, or -1 if
     * the element already exists. */
    if ((index = _dict_key_index(d, key, dict_hash_key(d,key), existing)) == -1) {
        return NULL;
    }

    /* Allocate the memory and store the new entry.
     * Insert the element in top, with the assumption that in a database
     * system it is more likely that recently added entries are accessed
     * more frequently. */
    htidx = dict_is_rehashing(d) ? 1 : 0;
    size_t metasize = dict_get_meta_data_size(d);
    entry = zmalloc(sizeof(*entry) + metasize);
    if (metasize > 0) {
        memset(dict_get_meta_data(entry), 0, metasize);
    }
    entry->next = d->ht_table[htidx][index];
    d->ht_table[htidx][index] = entry;
    d->ht_used[htidx]++;

    /* Set the hash entry fields. */
    dict_set_key(d, entry, key);
    return entry;
}

/* Add or Find:
 * dict_add_or_find() is simply a version of dict_add_raw() that always
 * returns the hash entry of the specified key, even if the key already
 * exists and can't be added (in that case the entry of the already
 * existing key is returned.)
 *
 * See dict_add_raw() for more information. */
dict_entry_t*dict_add_or_find(dict_t*d, void *key) {
    dict_entry_t*entry, *existing;
    entry = dict_add_raw(d,key,&existing);
    return entry ? entry : existing;
}





/* TODO: clz optimization */
/* Our hash table capability is a power of two */
static signed char _dict_next_exp(unsigned long size)
{
    unsigned char e = DICT_HT_INITIAL_EXP;

    if (size >= LONG_MAX) return (8*sizeof(long)-1);
    while(1) {
        if (((unsigned long)1<<e) >= size)
            return e;
        e++;
    }
}




/* Because we may need to allocate huge memory chunk at once when dict
 * expands, we will check this allocation is allowed or not if the dict
 * type has expandAllowed member function. */
static int dict_expand_allowed(dict_t*d) {
    if (d->type->expandAllowed == NULL) return 1;
    return d->type->expandAllowed(
                    DICTHT_SIZE(_dict_next_exp(d->ht_used[0] + 1)) * sizeof(dict_entry_t*),
                    (double)d->ht_used[0] / DICTHT_SIZE(d->ht_size_exp[0]));
}

/* Expand the hash table if needed */
static int _dict_expand_if_needed(dict_t*d)
{
    /* Incremental rehashing already in progress. Return. */
    if (dict_is_rehashing(d)) return DICT_OK;

    /* If the hash table is empty expand it to the initial size. */
    if (DICTHT_SIZE(d->ht_size_exp[0]) == 0) return dict_expand(d, DICT_HT_INITIAL_SIZE);

    /* If we reached the 1:1 ratio, and we are allowed to resize the hash
     * table (global setting) or we should avoid it but the ratio between
     * elements/buckets is over the "safe" threshold, we resize doubling
     * the number of buckets. */
    if (d->ht_used[0] >= DICTHT_SIZE(d->ht_size_exp[0]) &&
        (dict_can_resize ||
         d->ht_used[0]/ DICTHT_SIZE(d->ht_size_exp[0]) > dict_force_resize_ratio) &&
        dict_expand_allowed(d))
    {
        return dict_expand(d, d->ht_used[0] + 1);
    }
    return DICT_OK;
}

/* Returns the index of a free slot that can be populated with
 * a hash entry for the given 'key'.
 * If the key already exists, -1 is returned
 * and the optional output parameter may be filled.
 *
 * Note that if we are in the process of rehashing the hash table, the
 * index is always returned in the context of the second (new) hash table. */
static long _dict_key_index(dict_t*d, const void *key, uint64_t hash, dict_entry_t**existing)
{
    unsigned long idx, table;
    dict_entry_t*he;
    if (existing) *existing = NULL;

    /* Expand the hash table if needed */
    if (_dict_expand_if_needed(d) == DICT_ERR) {
        return -1;
    }
    for (table = 0; table <= 1; table++) {
        idx = hash & DICTHT_SIZE_MASK(d->ht_size_exp[table]);
        /* Search if this slot does not already contain the given key */
        he = d->ht_table[table][idx];
        while(he) {
            if (key==he->key || dict_compare_keys(d, key, he->key)) {
                if (existing) *existing = he;
                return -1;
            }
            he = he->next;
        }
        if (!dict_is_rehashing(d)) break;
    }
    return idx;
}


dict_entry_t*dict_find(dict_t*d, const void *key)
{
    dict_entry_t*he;
    uint64_t h, idx, table;

    if (dict_size(d) == 0) return NULL; /* dict_tis empty */
    if (dict_is_rehashing(d)) _dict_rehash_step(d);
    h = dict_hash_key(d, key);
    for (table = 0; table <= 1; table++) {
        idx = h & DICTHT_SIZE_MASK(d->ht_size_exp[table]);
        he = d->ht_table[table][idx];
        while(he) {
            if (key==he->key || dict_compare_keys(d, key, he->key))
                return he;
            he = he->next;
        }
        if (!dict_is_rehashing(d)) return NULL;
    }
    return NULL;
}

dict_entry_t* dict_add_get(dict_t* d, void* key, void* val) {
    dict_entry_t*entry = dict_add_raw(d,key,NULL);
    if (NULL == entry) {
        return NULL;
    }
    dict_set_val(d, entry, val);
    return entry;
}

int dict_add(dict_t*d, void *key, void *val) {
    dict_entry_t* entry =  dict_add_get(d, key, val);
    if (!entry) return DICT_ERR;
    return DICT_OK;
}


/* A fingerprint is a 64 bit number that represents the state of the dictionary
 * at a given time, it's just a few dict_tproperties xored together.
 * When an unsafe iterator is initialized, we get the dict_tfingerprint, and check
 * the fingerprint again when the iterator is released.
 * If the two fingerprints are different it means that the user of the iterator
 * performed forbidden operations against the dictionary while iterating. */
unsigned long long dictFingerprint(dict_t*d) {
    unsigned long long integers[6], hash = 0;
    int j;

    integers[0] = (long) d->ht_table[0];
    integers[1] = d->ht_size_exp[0];
    integers[2] = d->ht_used[0];
    integers[3] = (long) d->ht_table[1];
    integers[4] = d->ht_size_exp[1];
    integers[5] = d->ht_used[1];

    /* We hash N integers by summing every successive integer with the integer
     * hashing of the previous sum. Basically:
     *
     * Result = hash(hash(hash(int1)+int2)+int3) ...
     *
     * This way the same set of integers in a different order will (likely) hash
     * to a different number. */
    for (j = 0; j < 6; j++) {
        hash += integers[j];
        /* For the hashing step we use Tomas Wang's 64 bit integer hash. */
        hash = (~hash) + (hash << 21); // hash = (hash << 21) - hash - 1;
        hash = hash ^ (hash >> 24);
        hash = (hash + (hash << 3)) + (hash << 8); // hash * 265
        hash = hash ^ (hash >> 14);
        hash = (hash + (hash << 2)) + (hash << 4); // hash * 21
        hash = hash ^ (hash >> 28);
        hash = hash + (hash << 31);
    }
    return hash;
}

int hashTable_min_fill = HASHTABLE_MIN_FILL;
/**
 *  如果数据量/总量小于{hashTable_min_fill}%的时候进行可以缩容
 *  这里值目前是10%
 */
int ht_needs_resize(dict_t*dict) {
    long long size, used;

    size = dict_slots(dict);
    used = dict_size(dict);
    return (size > DICT_HT_INITIAL_SIZE &&
            (used*100/size < hashTable_min_fill));
}

/* Resize the table to the minimal size that contains all the elements,
 * but with the invariant of a USED/BUCKETS ratio near to <= 1 */
int dict_resize(dict_t*d)
{
    unsigned long minimal;

    if (dict_can_resize != DICT_RESIZE_ENABLE || dict_is_rehashing(d)) return DICT_ERR;
    minimal = d->ht_used[0];
    if (minimal < DICT_HT_INITIAL_SIZE)
        minimal = DICT_HT_INITIAL_SIZE;
    return dict_expand(d, minimal);
}

/* Returns 1 if the entry pointer is a pointer to a key, rather than to an
 * allocated entry. Returns 0 otherwise. */
static inline int entryIsKey(const dict_entry_t*de) {
    return (uintptr_t)(void *)de & 1;
}

/* --------------------- dict_entry_tpointer bit tricks ----------------------  */

/* The 3 least significant bits in a pointer to a dict_entry_tdetermines what the
 * pointer actually points to. If the least bit is set, it's a key. Otherwise,
 * the bit pattern of the least 3 significant bits mark the kind of entry. */

#define ENTRY_PTR_MASK     7 /* 111 */
#define ENTRY_PTR_NORMAL   0 /* 000 */
#define ENTRY_PTR_NO_VALUE 2 /* 010 */

/* Returns 1 if the pointer is actually a pointer to a dict_entry_tstruct. Returns
 * 0 otherwise. */
// static inline int entryIsNormal(const dict_entry_t*de) {
//     return ((uintptr_t)(void *)de & ENTRY_PTR_MASK) == ENTRY_PTR_NORMAL;
// }

typedef struct {
    void *key;
    dict_entry_t*next;
} dict_entry_tNoValue;

/* Returns 1 if the entry is a special entry with key and next, but without
 * value. Returns 0 otherwise. */
static inline int entryIsNoValue(const dict_entry_t*de) {
    return ((uintptr_t)(void *)de & ENTRY_PTR_MASK) == ENTRY_PTR_NO_VALUE;
}

static inline void *decodeMaskedPtr(const dict_entry_t*de) {
    assert(!entryIsKey(de));
    return (void *)((uintptr_t)(void *)de & ~ENTRY_PTR_MASK);
}

/* Decodes the pointer to an entry without value, when you know it is an entry
 * without value. Hint: Use entryIsNoValue to check. */
static inline dict_entry_tNoValue *decodeEntryNoValue(const dict_entry_t*de) {
    return decodeMaskedPtr(de);
}

void *dictGetKey(const dict_entry_t*de) {
    if (entryIsKey(de)) return (void*)de;
    if (entryIsNoValue(de)) return decodeEntryNoValue(de)->key;
    return de->key;
}


/* Returns the 'next' field of the entry or NULL if the entry doesn't have a
 * 'next' field. */
static dict_entry_t*dictGetNext(const dict_entry_t*de) {
    if (entryIsKey(de)) return NULL; /* there's no next */
    if (entryIsNoValue(de)) return decodeEntryNoValue(de)->next;
    return de->next;
}



static void dictSetNext(dict_entry_t*de, dict_entry_t*next) {
    assert(!entryIsKey(de));
    if (entryIsNoValue(de)) {
        dict_entry_tNoValue *entry = decodeEntryNoValue(de);
        entry->next = next;
    } else {
        de->next = next;
    }
}



/* You need to call this function to really free the entry after a call
 * to dictUnlink(). It's safe to call this function with 'he' = NULL. */
void dictFreeUnlinkedEntry(dict_t*d, dict_entry_t*he) {
    if (he == NULL) return;
    dict_free_key(d, he);
    dict_free_val(d, he);
    if (!entryIsKey(he)) zfree(decodeMaskedPtr(he));
}


/* Search and remove an element. This is a helper function for
 * dict_delete_key() and dictUnlink(), please check the top comment
 * of those functions. */
static dict_entry_t*dict_generic_delete(dict_t*d, const void *key, int nofree) {
    uint64_t h, idx;
    dict_entry_t*he, *prevHe;
    int table;

    /* dict_tis empty */
    if (dict_size(d) == 0) return NULL;

    if (dict_is_rehashing(d)) _dict_rehash_step(d);
    h = dict_hash_key(d, key);

    for (table = 0; table <= 1; table++) {
        idx = h & DICTHT_SIZE_MASK(d->ht_size_exp[table]);
        he = d->ht_table[table][idx];
        prevHe = NULL;
        while(he) {
            void *he_key = dictGetKey(he);
            if (key == he_key || dict_compare_keys(d, key, he_key)) {
                /* Unlink the element from the list */
                if (prevHe)
                    dictSetNext(prevHe, dictGetNext(he));
                else
                    d->ht_table[table][idx] = dictGetNext(he);
                if (!nofree) {
                    dictFreeUnlinkedEntry(d, he);
                }
                d->ht_used[table]--;
                return he;
            }
            prevHe = he;
            he = dictGetNext(he);
        }
        if (!dict_is_rehashing(d)) break;
    }
    return NULL; /* not found */
}
/* Remove an element, returning DICT_OK on success or DICT_ERR if the
 * element was not found. */
int dict_delete_key(dict_t*ht, const void *key) {
    return dict_generic_delete(ht,key,0) ? DICT_OK : DICT_ERR;
}

void* dict_fetch_value(dict_t *d, const void *key) {
    dict_entry_t* he;

    he = dict_find(d, key);
    return he ? dict_get_entry_val(he) : NULL;
}

#define dict_iterator_dict_ptr(iter)  ((dict_t*)iter->sup.sup.data) 

bool protected_dict_iterator_has_next(latte_iterator_t* it) {
    dict_iterator_t* iter = (dict_iterator_t*)it;
    if (iter->index == -2) return false;
    while(1) {
        if (iter->entry == NULL) {
            if (iter->index == -1 && iter->table == 0) {
                if (iter->safe)
                    dict_pause_rehashing(dict_iterator_dict_ptr(iter));
                else
                    iter->fingerprint = dictFingerprint(dict_iterator_dict_ptr(iter));
            }
            iter->index++;
            if (iter->index >= (long) DICTHT_SIZE(dict_iterator_dict_ptr(iter)->ht_size_exp[iter->table])) {
                if (dict_is_rehashing(dict_iterator_dict_ptr(iter)) && iter->table == 0) {
                    iter->table++;
                    iter->index = 0;
                } else {
                    break;
                }
            }
            iter->entry = dict_iterator_dict_ptr(iter)->ht_table[iter->table][iter->index];
        } else {
            iter->entry = iter->nextEntry;
        }
        if (iter->entry) {
            /* We need to save the 'next' here, the iterator user
             * may delete the entry we are returning. */
            iter->nextEntry = iter->entry->next;
            iter->readed = false;
            return true;
        }
    }
    iter->index = -2;
    return false;
}

void* protected_dict_iterator_next(latte_iterator_t* it) {
    dict_iterator_t* iter = (dict_iterator_t*)it;
    if (iter->index == -2) return NULL;
    if (iter->readed == true) { //防止用户不使用has_next 直接用next方法
        if (!protected_dict_iterator_has_next(it)) return NULL;
    }
    iterator_pair_key(iter) = dict_get_entry_key(iter->entry);
    iterator_pair_value(iter) = dict_get_entry_val(iter->entry);
    iter->readed = true;
    return iterator_pair_ptr(iter);
}

void protected_dict_iterator_delete(latte_iterator_t* it) {
    dict_iterator_t* iter = (dict_iterator_t*)it;
    if (!(iter->index == -1 && iter->table == 0)) {
        if (iter->safe)
            dict_resume_rehashing(dict_iterator_dict_ptr(iter));
        else
            assert(iter->fingerprint == dictFingerprint(dict_iterator_dict_ptr(iter)));
    }
    zfree(iter);
}

latte_iterator_func dict_iterator_func = {
    .has_next = protected_dict_iterator_has_next,
    .next = protected_dict_iterator_next,
    .release = protected_dict_iterator_delete
};

latte_iterator_t* dict_get_latte_iterator(dict_t *d) {
    dict_iterator_t* iter = zmalloc(sizeof(dict_iterator_t));
    latte_iterator_pair_init(&iter->sup, &dict_iterator_func, d);
    iter->table = 0;
    iter->index = -1;
    iter->safe = 0;
    iter->entry = NULL;
    iter->nextEntry = NULL;
    iter->readed = false;
    return (latte_iterator_t*)iter;
}

int private_dict_cmp(dict_t* a, dict_t* b, cmp_func cmp) {
    if (dict_size(a) != dict_size(b)) {
        return dict_size(a) - dict_size(b);
    }
    latte_iterator_t* it = dict_get_latte_iterator(a);
    int result = 0;
    while(latte_iterator_has_next(it)) {
        latte_pair_t* pair = latte_iterator_next(it);
        void* key = latte_pair_key(pair);
        dict_entry_t* entry = dict_find(b, key);
        if (entry == NULL) {
            result = -1;
            goto end;
        }
        if ( (result = cmp(latte_pair_value(pair),  dict_get_entry_val(entry))) != 0) {
            goto end;
        }
    }
end:
    latte_iterator_delete(it);
    return result;
}