#include "int_set.h"
#include "endianconv/endianconv.h"
#include "zmalloc/zmalloc.h"
#include <assert.h>
#include <stdlib.h>
#define INTSET_ENC_INT16 (sizeof(int16_t))
int_set_t* int_set_new() {
    int_set_t* is = zmalloc(sizeof(int_set_t));
    is->encoding = intrev32ifbe(INTSET_ENC_INT16);
    is->length = 0;
    return is;
}

/* Return the value at pos, given an encoding. */
static int64_t _int_set_get_encoded(int_set_t *is, int pos, uint8_t enc) {
    int64_t v64;
    int32_t v32;
    int16_t v16;

    if (enc == INTSET_ENC_INT64) {
        memcpy(&v64,((int64_t*)is->contents)+pos,sizeof(v64));
        memrev64ifbe(&v64);
        return v64;
    } else if (enc == INTSET_ENC_INT32) {
        memcpy(&v32,((int32_t*)is->contents)+pos,sizeof(v32));
        memrev32ifbe(&v32);
        return v32;
    } else {
        memcpy(&v16,((int16_t*)is->contents)+pos,sizeof(v16));
        memrev16ifbe(&v16);
        return v16;
    }
}

/* Return the required encoding for the provided value. */
uint8_t _int_set_value_encoding(int64_t v) {
    if (v < INT32_MIN || v > INT32_MAX)
        return INTSET_ENC_INT64;
    else if (v < INT16_MIN || v > INT16_MAX)
        return INTSET_ENC_INT32;
    else
        return INTSET_ENC_INT16;
}


/* Resize the intset */
static int_set_t *int_set_resize(int_set_t *is, uint32_t len) {
    uint64_t size = (uint64_t)len*intrev32ifbe(is->encoding);
    assert(size <= SIZE_MAX - sizeof(int_set_t));
    is = zrealloc(is,sizeof(int_set_t)+size);
    return is;
}

/* Set the value at pos, using the configured encoding. */
static void _int_set_set(int_set_t *is, int pos, int64_t value) {
    uint32_t encoding = intrev32ifbe(is->encoding);

    if (encoding == INTSET_ENC_INT64) {
        ((int64_t*)is->contents)[pos] = value;
        memrev64ifbe(((int64_t*)is->contents)+pos);
    } else if (encoding == INTSET_ENC_INT32) {
        ((int32_t*)is->contents)[pos] = value;
        memrev32ifbe(((int32_t*)is->contents)+pos);
    } else {
        ((int16_t*)is->contents)[pos] = value;
        memrev16ifbe(((int16_t*)is->contents)+pos);
    }
}

/* Upgrades the intset to a larger encoding and inserts the given integer. */
static int_set_t *int_set_upgrade_and_add(int_set_t *is, int64_t value) {
    uint8_t curenc = intrev32ifbe(is->encoding);
    uint8_t newenc = _int_set_value_encoding(value);
    int length = intrev32ifbe(is->length);
    int prepend = value < 0 ? 1 : 0;

    /* First set new encoding and resize */
    is->encoding = intrev32ifbe(newenc);
    is = int_set_resize(is,intrev32ifbe(is->length)+1);

    /* Upgrade back-to-front so we don't overwrite values.
     * Note that the "prepend" variable is used to make sure we have an empty
     * space at either the beginning or the end of the intset. */
    while(length--)
        _int_set_set(is,length+prepend,_int_set_get_encoded(is,length,curenc));

    /* Set the value at the beginning or the end. */
    if (prepend)
        _int_set_set(is,0,value);
    else
        _int_set_set(is,intrev32ifbe(is->length),value);
    is->length = intrev32ifbe(intrev32ifbe(is->length)+1);
    return is;
}

/* Return the value at pos, using the configured encoding. */
static int64_t _int_set_get(int_set_t *is, int pos) {
    return _int_set_get_encoded(is,pos,intrev32ifbe(is->encoding));
}



static void int_set_move_tail(int_set_t *is, uint32_t from, uint32_t to) {
    void *src, *dst;
    uint32_t bytes = intrev32ifbe(is->length)-from;
    uint32_t encoding = intrev32ifbe(is->encoding);

    if (encoding == INTSET_ENC_INT64) {
        src = (int64_t*)is->contents+from;
        dst = (int64_t*)is->contents+to;
        bytes *= sizeof(int64_t);
    } else if (encoding == INTSET_ENC_INT32) {
        src = (int32_t*)is->contents+from;
        dst = (int32_t*)is->contents+to;
        bytes *= sizeof(int32_t);
    } else {
        src = (int16_t*)is->contents+from;
        dst = (int16_t*)is->contents+to;
        bytes *= sizeof(int16_t);
    }
    memmove(dst,src,bytes);
}

/* Search for the position of "value". Return 1 when the value was found and
 * sets "pos" to the position of the value within the intset. Return 0 when
 * the value is not present in the intset and sets "pos" to the position
 * where "value" can be inserted. */
uint8_t int_set_search(int_set_t *is, int64_t value, uint32_t *pos) {
    int min = 0, max = intrev32ifbe(is->length)-1, mid = -1;
    int64_t cur = -1;

    /* The value can never be found when the set is empty */
    if (intrev32ifbe(is->length) == 0) {
        if (pos) *pos = 0;
        return 0;
    } else {
        /* Check for the case where we know we cannot find the value,
         * but do know the insert position. */
        if (value > _int_set_get(is,max)) {
            if (pos) *pos = intrev32ifbe(is->length);
            return 0;
        } else if (value < _int_set_get(is,0)) {
            if (pos) *pos = 0;
            return 0;
        }
    }

    while(max >= min) {
        mid = ((unsigned int)min + (unsigned int)max) >> 1;
        cur = _int_set_get(is,mid);
        if (value > cur) {
            min = mid+1;
        } else if (value < cur) {
            max = mid-1;
        } else {
            break;
        }
    }

    if (value == cur) {
        if (pos) *pos = mid;
        return 1;
    } else {
        if (pos) *pos = min;
        return 0;
    }
}

int_set_t* int_set_add(int_set_t* is, int64_t value, uint8_t *success) {
    uint8_t valenc = _int_set_value_encoding(value);
    uint32_t pos;
    if (success) *success = 1;

    /* Upgrade encoding if necessary. If we need to upgrade, we know that
     * this value should be either appended (if > 0) or prepended (if < 0),
     * because it lies outside the range of existing values. */
    if (valenc > intrev32ifbe(is->encoding)) {
        /* This always succeeds, so we don't need to curry *success. */
        return int_set_upgrade_and_add(is,value);
    } else {
        /* Abort if the value is already present in the set.
         * This call will populate "pos" with the right position to insert
         * the value when it cannot be found. */
        if (int_set_search(is,value,&pos)) {
            if (success) *success = 0;
            return is;
        }

        is = int_set_resize(is,intrev32ifbe(is->length)+1);
        if (pos < intrev32ifbe(is->length)) int_set_move_tail(is,pos,pos+1);
    }

    _int_set_set(is,pos,value);
    is->length = intrev32ifbe(intrev32ifbe(is->length)+1);
    return is;
}

/* Delete integer from intset */
int_set_t *int_set_remove(int_set_t *is, int64_t value, int *success) {
    uint8_t valenc = _int_set_value_encoding(value);
    uint32_t pos;
    if (success) *success = 0;

    if (valenc <= intrev32ifbe(is->encoding) && int_set_search(is,value,&pos)) {
        uint32_t len = intrev32ifbe(is->length);

        /* We know we can delete */
        if (success) *success = 1;

        /* Overwrite value with tail and update length */
        if (pos < (len-1)) int_set_move_tail(is,pos+1,pos);
        is = int_set_resize(is,len-1);
        is->length = intrev32ifbe(len-1);
    }
    return is;
}

/* Determine whether a value belongs to this set */
uint8_t int_set_find(int_set_t *is, int64_t value) {
    uint8_t valenc = _int_set_value_encoding(value);
    return valenc <= intrev32ifbe(is->encoding) && int_set_search(is,value,NULL);
}

/* Return random member */
int64_t int_set_random(int_set_t *is) {
    uint32_t len = intrev32ifbe(is->length);
    assert(len); /* avoid division by zero on corrupt intset payload. */
    return _int_set_get(is,rand()%len);
}

/* Get the value at the given position. When this position is
 * out of range the function returns 0, when in range it returns 1. */
uint8_t int_set_get(int_set_t *is, uint32_t pos, int64_t *value) {
    if (pos < intrev32ifbe(is->length)) {
        *value = _int_set_get(is,pos);
        return 1;
    }
    return 0;
}

/* Return intset length */
uint32_t int_set_len(const int_set_t *is) {
    return intrev32ifbe(is->length);
}

/* Return intset blob size in bytes. */
size_t int_set_blob_len(int_set_t *is) {
    return sizeof(int_set_t)+(size_t)intrev32ifbe(is->length)*intrev32ifbe(is->encoding);
}

int int_set_validate_integrity(const unsigned char *p, size_t size, int deep) {
    int_set_t *is = (int_set_t *)p;
    /* check that we can actually read the header. */
    if (size < sizeof(*is))
        return 0;

    uint32_t encoding = intrev32ifbe(is->encoding);

    size_t record_size;
    if (encoding == INTSET_ENC_INT64) {
        record_size = INTSET_ENC_INT64;
    } else if (encoding == INTSET_ENC_INT32) {
        record_size = INTSET_ENC_INT32;
    } else if (encoding == INTSET_ENC_INT16){
        record_size = INTSET_ENC_INT16;
    } else {
        return 0;
    }

    /* check that the size matchies (all records are inside the buffer). */
    uint32_t count = intrev32ifbe(is->length);
    if (sizeof(*is) + count*record_size != size)
        return 0;

    /* check that the set is not empty. */
    if (count==0)
        return 0;

    if (!deep)
        return 1;

    /* check that there are no dup or out of order records. */
    int64_t prev = _int_set_get(is,0);
    for (uint32_t i=1; i<count; i++) {
        int64_t cur = _int_set_get(is,i);
        if (cur <= prev)
            return 0;
        prev = cur;
    }

    return 1;
}