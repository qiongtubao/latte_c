//
// Created by dong on 23-5-22.
//

#include "listpack.h"
#include <stdio.h>
#include "utils/utils.h"
#include <string.h>
#include <stdlib.h>

/* Create a new, empty listpack.
 * On success the new listpack is returned, otherwise an error is returned.
 * Pre-allocate at least `capacity` bytes of memory,
 * over-allocated memory can be shrunk by `lpShrinkToFit`.
 * */
#define LP_HDR_SIZE 6       /* 32 bit total len + 16 bit number of elements. */
#define LP_HDR_NUMELE_UNKNOWN UINT16_MAX
#define LP_MAX_INT_ENCODING_LEN 9
#define LP_MAX_BACKLEN_SIZE 5
#define LP_ENCODING_INT 0
#define LP_ENCODING_STRING 1


#define LP_EOF 0xFF
/* Validates that 'p' is not outside the listpack.
 * All function that return a pointer to an element in the listpack will assert
 * that this element is valid, so it can be freely used.
 * Generally functions such lpNext and lpDelete assume the input pointer is
 * already validated (since it's the return value of another function). */
#define ASSERT_INTEGRITY(lp, p) do { \
    assert((p) >= (lp)+LP_HDR_SIZE && (p) < (lp)+lpGetTotalBytes((lp))); \
} while (0)

#define lpSetTotalBytes(p,v) do { \
    (p)[0] = (v)&0xff; \
    (p)[1] = ((v)>>8)&0xff; \
    (p)[2] = ((v)>>16)&0xff; \
    (p)[3] = ((v)>>24)&0xff; \
} while(0)

#define lpSetNumElements(p,v) do { \
    (p)[4] = (v)&0xff; \
    (p)[5] = ((v)>>8)&0xff; \
} while(0)


#define lpGetTotalBytes(p)           (((uint32_t)(p)[0]<<0) | \
                                      ((uint32_t)(p)[1]<<8) | \
                                      ((uint32_t)(p)[2]<<16) | \
                                      ((uint32_t)(p)[3]<<24))

/* Validates that 'p' is not outside the listpack.
 * All function that return a pointer to an element in the listpack will assert
 * that this element is valid, so it can be freely used.
 * Generally functions such lpNext and lpDelete assume the input pointer is
 * already validated (since it's the return value of another function). */
#define ASSERT_INTEGRITY(lp, p) do { \
    assert((p) >= (lp)+LP_HDR_SIZE && (p) < (lp)+lpGetTotalBytes((lp))); \
} while (0)

static inline void lpAssertValidEntry(unsigned char* lp, size_t lpbytes, unsigned char *p);


#define LP_ENCODING_7BIT_UINT 0
#define LP_ENCODING_7BIT_UINT_MASK 0x80
#define LP_ENCODING_IS_7BIT_UINT(byte) (((byte)&LP_ENCODING_7BIT_UINT_MASK)==LP_ENCODING_7BIT_UINT)
#define LP_ENCODING_7BIT_UINT_ENTRY_SIZE 2

#define LP_ENCODING_6BIT_STR 0x80
#define LP_ENCODING_6BIT_STR_MASK 0xC0
#define LP_ENCODING_IS_6BIT_STR(byte) (((byte)&LP_ENCODING_6BIT_STR_MASK)==LP_ENCODING_6BIT_STR)

#define LP_ENCODING_13BIT_INT 0xC0
#define LP_ENCODING_13BIT_INT_MASK 0xE0
#define LP_ENCODING_IS_13BIT_INT(byte) (((byte)&LP_ENCODING_13BIT_INT_MASK)==LP_ENCODING_13BIT_INT)
#define LP_ENCODING_13BIT_INT_ENTRY_SIZE 3

#define LP_ENCODING_12BIT_STR 0xE0
#define LP_ENCODING_12BIT_STR_MASK 0xF0
#define LP_ENCODING_IS_12BIT_STR(byte) (((byte)&LP_ENCODING_12BIT_STR_MASK)==LP_ENCODING_12BIT_STR)

#define LP_ENCODING_16BIT_INT 0xF1
#define LP_ENCODING_16BIT_INT_MASK 0xFF
#define LP_ENCODING_IS_16BIT_INT(byte) (((byte)&LP_ENCODING_16BIT_INT_MASK)==LP_ENCODING_16BIT_INT)
#define LP_ENCODING_16BIT_INT_ENTRY_SIZE 4

#define LP_ENCODING_24BIT_INT 0xF2
#define LP_ENCODING_24BIT_INT_MASK 0xFF
#define LP_ENCODING_IS_24BIT_INT(byte) (((byte)&LP_ENCODING_24BIT_INT_MASK)==LP_ENCODING_24BIT_INT)
#define LP_ENCODING_24BIT_INT_ENTRY_SIZE 5

#define LP_ENCODING_32BIT_INT 0xF3
#define LP_ENCODING_32BIT_INT_MASK 0xFF
#define LP_ENCODING_IS_32BIT_INT(byte) (((byte)&LP_ENCODING_32BIT_INT_MASK)==LP_ENCODING_32BIT_INT)
#define LP_ENCODING_32BIT_INT_ENTRY_SIZE 6

#define LP_ENCODING_64BIT_INT 0xF4
#define LP_ENCODING_64BIT_INT_MASK 0xFF
#define LP_ENCODING_IS_64BIT_INT(byte) (((byte)&LP_ENCODING_64BIT_INT_MASK)==LP_ENCODING_64BIT_INT)
#define LP_ENCODING_64BIT_INT_ENTRY_SIZE 10

#define LP_ENCODING_32BIT_STR 0xF0
#define LP_ENCODING_32BIT_STR_MASK 0xFF
#define LP_ENCODING_IS_32BIT_STR(byte) (((byte)&LP_ENCODING_32BIT_STR_MASK)==LP_ENCODING_32BIT_STR)

#define LP_EOF 0xFF
#define LP_ENCODING_6BIT_STR_LEN(p) ((p)[0] & 0x3F)
#define LP_ENCODING_12BIT_STR_LEN(p) ((((p)[0] & 0xF) << 8) | (p)[1])
#define LP_ENCODING_32BIT_STR_LEN(p) (((uint32_t)(p)[1]<<0) | \
                                      ((uint32_t)(p)[2]<<8) | \
                                      ((uint32_t)(p)[3]<<16) | \
                                      ((uint32_t)(p)[4]<<24))

#define lpGetTotalBytes(p)           (((uint32_t)(p)[0]<<0) | \
                                      ((uint32_t)(p)[1]<<8) | \
                                      ((uint32_t)(p)[2]<<16) | \
                                      ((uint32_t)(p)[3]<<24))

#define lpGetNumElements(p)          (((uint32_t)(p)[4]<<0) | \
                                      ((uint32_t)(p)[5]<<8))
#define lpSetTotalBytes(p,v) do { \
    (p)[0] = (v)&0xff; \
    (p)[1] = ((v)>>8)&0xff; \
    (p)[2] = ((v)>>16)&0xff; \
    (p)[3] = ((v)>>24)&0xff; \
} while(0)

#define lpSetNumElements(p,v) do { \
    (p)[4] = (v)&0xff; \
    (p)[5] = ((v)>>8)&0xff; \
} while(0)


/* Return the encoded length of the listpack element pointed by 'p'.
 * This includes the encoding byte, length bytes, and the element data itself.
 * If the element encoding is wrong then 0 is returned.
 * Note that this method may access additional bytes (in case of 12 and 32 bit
 * str), so should only be called when we know 'p' was already validated by
 * lpCurrentEncodedSizeBytes or ASSERT_INTEGRITY_LEN (possibly since 'p' is
 * a return value of another function that validated its return. */
static inline uint32_t lpCurrentEncodedSizeUnsafe(unsigned char *p) {
    if (LP_ENCODING_IS_7BIT_UINT(p[0])) return 1;
    if (LP_ENCODING_IS_6BIT_STR(p[0])) return 1+LP_ENCODING_6BIT_STR_LEN(p);
    if (LP_ENCODING_IS_13BIT_INT(p[0])) return 2;
    if (LP_ENCODING_IS_16BIT_INT(p[0])) return 3;
    if (LP_ENCODING_IS_24BIT_INT(p[0])) return 4;
    if (LP_ENCODING_IS_32BIT_INT(p[0])) return 5;
    if (LP_ENCODING_IS_64BIT_INT(p[0])) return 9;
    if (LP_ENCODING_IS_12BIT_STR(p[0])) return 2+LP_ENCODING_12BIT_STR_LEN(p);
    if (LP_ENCODING_IS_32BIT_STR(p[0])) return 5+LP_ENCODING_32BIT_STR_LEN(p);
    if (p[0] == LP_EOF) return 1;
    return 0;
}

/* Store a reverse-encoded variable length field, representing the length
 * of the previous element of size 'l', in the target buffer 'buf'.
 * The function returns the number of bytes used to encode it, from
 * 1 to 5. If 'buf' is NULL the function just returns the number of bytes
 * needed in order to encode the backlen. */
static inline unsigned long lpEncodeBacklen(unsigned char *buf, uint64_t l) {
    if (l <= 127) {
        if (buf) buf[0] = l;
        return 1;
    } else if (l < 16383) {
        if (buf) {
            buf[0] = l>>7;
            buf[1] = (l&127)|128;
        }
        return 2;
    } else if (l < 2097151) {
        if (buf) {
            buf[0] = l>>14;
            buf[1] = ((l>>7)&127)|128;
            buf[2] = (l&127)|128;
        }
        return 3;
    } else if (l < 268435455) {
        if (buf) {
            buf[0] = l>>21;
            buf[1] = ((l>>14)&127)|128;
            buf[2] = ((l>>7)&127)|128;
            buf[3] = (l&127)|128;
        }
        return 4;
    } else {
        if (buf) {
            buf[0] = l>>28;
            buf[1] = ((l>>21)&127)|128;
            buf[2] = ((l>>14)&127)|128;
            buf[3] = ((l>>7)&127)|128;
            buf[4] = (l&127)|128;
        }
        return 5;
    }
}

/* Skip the current entry returning the next. It is invalid to call this
 * function if the current element is the EOF element at the end of the
 * listpack, however, while this function is used to implement lpNext(),
 * it does not return NULL when the EOF element is encountered. */
unsigned char *lpSkip(unsigned char *p) {
    unsigned long entrylen = lpCurrentEncodedSizeUnsafe(p);
    entrylen += lpEncodeBacklen(NULL,entrylen);
    p += entrylen;
    return p;
}

unsigned char *lp_new(size_t capacity) {
    unsigned char *lp = lp_malloc(capacity > LP_HDR_SIZE+1 ? capacity : LP_HDR_SIZE+1);
    if (lp == NULL) return NULL;
    lpSetTotalBytes(lp, LP_HDR_SIZE + 1);
    lpSetNumElements(lp,0);
    lp[LP_HDR_SIZE]=LP_EOF;
    return lp;
}

/* Free the specified listpack. */
void lpFree(unsigned char *lp) {
    lp_free(lp);
}

/* Return the total number of bytes the listpack is composed of. */
size_t lp_bytes(unsigned char *lp) {
    return lpGetTotalBytes(lp);
}


/* Return a pointer to the first element of the listpack, or NULL if the
 * listpack has no elements. */
unsigned char *lpFirst(unsigned char *lp) {
    unsigned char *p = lp + LP_HDR_SIZE; /* Skip the header. */
    if (p[0] == LP_EOF) return NULL;
    lpAssertValidEntry(lp, lp_bytes(lp), p);
    return p;
}

/* Similar to the above, but validates the entire element length rather than just
 * it's pointer. */
#define ASSERT_INTEGRITY_LEN(lp, p, len) do { \
    assert((p) >= (lp)+LP_HDR_SIZE && (p)+(len) < (lp)+lpGetTotalBytes((lp))); \
} while (0)


/* Bytes needed for long -> str + '\0' */
#define LONG_STR_SIZE      21
/* Convert a string into a signed 64 bit integer.
 * The function returns 1 if the string could be parsed into a (non-overflowing)
 * signed 64 bit int, 0 otherwise. The 'value' will be set to the parsed value
 * when the function returns success.
 *
 * Note that this function demands that the string strictly represents
 * a int64 value: no spaces or other characters before or after the string
 * representing the number are accepted, nor zeroes at the start if not
 * for the string "0" representing the zero number.
 *
 * Because of its strictness, it is safe to use this function to check if
 * you can convert a string into a long long, and obtain back the string
 * from the number without any loss in the string representation. *
 *
 * -----------------------------------------------------------------------------
 *
 * Credits: this function was adapted from the Redis source code, file
 * "utils.c", function string2ll(), and is copyright:
 *
 * Copyright(C) 2011, Pieter Noordhuis
 * Copyright(C) 2011, Salvatore Sanfilippo
 *
 * The function is released under the BSD 3-clause license.
 */
int lpStringToInt64(const char *s, unsigned long slen, int64_t *value) {
    const char *p = s;
    unsigned long plen = 0;
    int negative = 0;
    uint64_t v;

    /* Abort if length indicates this cannot possibly be an int */
    if (slen == 0 || slen >= LONG_STR_SIZE)
        return 0;

    /* Special case: first and only digit is 0. */
    if (slen == 1 && p[0] == '0') {
        if (value != NULL) *value = 0;
        return 1;
    }

    if (p[0] == '-') {
        negative = 1;
        p++; plen++;

        /* Abort on only a negative sign. */
        if (plen == slen)
            return 0;
    }

    /* First digit should be 1-9, otherwise the string should just be 0. */
    if (p[0] >= '1' && p[0] <= '9') {
        v = p[0]-'0';
        p++; plen++;
    } else {
        return 0;
    }

    while (plen < slen && p[0] >= '0' && p[0] <= '9') {
        if (v > (UINT64_MAX / 10)) /* Overflow. */
            return 0;
        v *= 10;

        if (v > (UINT64_MAX - (p[0]-'0'))) /* Overflow. */
            return 0;
        v += p[0]-'0';

        p++; plen++;
    }

    /* Return if not all bytes were used. */
    if (plen < slen)
        return 0;

    if (negative) {
        if (v > ((uint64_t)(-(INT64_MIN+1))+1)) /* Overflow. */
            return 0;
        if (value != NULL) *value = -v;
    } else {
        if (v > INT64_MAX) /* Overflow. */
            return 0;
        if (value != NULL) *value = v;
    }
    return 1;
}

/* Stores the integer encoded representation of 'v' in the 'intenc' buffer. */
static inline void lpEncodeIntegerGetType(int64_t v, unsigned char *intenc, uint64_t *enclen) {
    if (v >= 0 && v <= 127) {
        /* Single byte 0-127 integer. */
        intenc[0] = v;
        *enclen = 1;
    } else if (v >= -4096 && v <= 4095) {
        /* 13 bit integer. */
        if (v < 0) v = ((int64_t)1<<13)+v;
        intenc[0] = (v>>8)|LP_ENCODING_13BIT_INT;
        intenc[1] = v&0xff;
        *enclen = 2;
    } else if (v >= -32768 && v <= 32767) {
        /* 16 bit integer. */
        if (v < 0) v = ((int64_t)1<<16)+v;
        intenc[0] = LP_ENCODING_16BIT_INT;
        intenc[1] = v&0xff;
        intenc[2] = v>>8;
        *enclen = 3;
    } else if (v >= -8388608 && v <= 8388607) {
        /* 24 bit integer. */
        if (v < 0) v = ((int64_t)1<<24)+v;
        intenc[0] = LP_ENCODING_24BIT_INT;
        intenc[1] = v&0xff;
        intenc[2] = (v>>8)&0xff;
        intenc[3] = v>>16;
        *enclen = 4;
    } else if (v >= -2147483648 && v <= 2147483647) {
        /* 32 bit integer. */
        if (v < 0) v = ((int64_t)1<<32)+v;
        intenc[0] = LP_ENCODING_32BIT_INT;
        intenc[1] = v&0xff;
        intenc[2] = (v>>8)&0xff;
        intenc[3] = (v>>16)&0xff;
        intenc[4] = v>>24;
        *enclen = 5;
    } else {
        /* 64 bit integer. */
        uint64_t uv = v;
        intenc[0] = LP_ENCODING_64BIT_INT;
        intenc[1] = uv&0xff;
        intenc[2] = (uv>>8)&0xff;
        intenc[3] = (uv>>16)&0xff;
        intenc[4] = (uv>>24)&0xff;
        intenc[5] = (uv>>32)&0xff;
        intenc[6] = (uv>>40)&0xff;
        intenc[7] = (uv>>48)&0xff;
        intenc[8] = uv>>56;
        *enclen = 9;
    }
}

/* Given an element 'ele' of size 'size', determine if the element can be
 * represented inside the listpack encoded as integer, and returns
 * LP_ENCODING_INT if so. Otherwise returns LP_ENCODING_STR if no integer
 * encoding is possible.
 *
 * If the LP_ENCODING_INT is returned, the function stores the integer encoded
 * representation of the element in the 'intenc' buffer.
 *
 * Regardless of the returned encoding, 'enclen' is populated by reference to
 * the number of bytes that the string or integer encoded element will require
 * in order to be represented. */
static inline int lpEncodeGetType(unsigned char *ele, uint32_t size, unsigned char *intenc, uint64_t *enclen) {
    int64_t v;
    if (lpStringToInt64((const char*)ele, size, &v)) {
        lpEncodeIntegerGetType(v, intenc, enclen);
        return LP_ENCODING_INT;
    } else {
        if (size < 64) *enclen = 1+size;
        else if (size < 4096) *enclen = 2+size;
        else *enclen = 5+(uint64_t)size;
        return LP_ENCODING_STRING;
    }
}


/* Encode the string element pointed by 's' of size 'len' in the target
 * buffer 's'. The function should be called with 'buf' having always enough
 * space for encoding the string. This is done by calling lpEncodeGetType()
 * before calling this function. */
static inline void lpEncodeString(unsigned char *buf, unsigned char *s, uint32_t len) {
    if (len < 64) {
        buf[0] = len | LP_ENCODING_6BIT_STR;
        memcpy(buf+1,s,len);
    } else if (len < 4096) {
        buf[0] = (len >> 8) | LP_ENCODING_12BIT_STR;
        buf[1] = len & 0xff;
        memcpy(buf+2,s,len);
    } else {
        buf[0] = LP_ENCODING_32BIT_STR;
        buf[1] = len & 0xff;
        buf[2] = (len >> 8) & 0xff;
        buf[3] = (len >> 16) & 0xff;
        buf[4] = (len >> 24) & 0xff;
        memcpy(buf+5,s,len);
    }
}

/* Insert, delete or replace the specified string element 'elestr' of length
 * 'size' or integer element 'eleint' at the specified position 'p', with 'p'
 * being a listpack element pointer obtained with lpFirst(), lpLast(), lpNext(),
 * lpPrev() or lpSeek().
 *
 * The element is inserted before, after, or replaces the element pointed
 * by 'p' depending on the 'where' argument, that can be LP_BEFORE, LP_AFTER
 * or LP_REPLACE.
 *
 * If both 'elestr' and `eleint` are NULL, the function removes the element
 * pointed by 'p' instead of inserting one.
 * If `eleint` is non-NULL, 'size' is the length of 'eleint', the function insert
 * or replace with a 64 bit integer, which is stored in the 'eleint' buffer.
 * If 'elestr` is non-NULL, 'size' is the length of 'elestr', the function insert
 * or replace with a string, which is stored in the 'elestr' buffer.
 *
 * Returns NULL on out of memory or when the listpack total length would exceed
 * the max allowed size of 2^32-1, otherwise the new pointer to the listpack
 * holding the new element is returned (and the old pointer passed is no longer
 * considered valid)
 *
 * If 'newp' is not NULL, at the end of a successful call '*newp' will be set
 * to the address of the element just added, so that it will be possible to
 * continue an interaction with lpNext() and lpPrev().
 *
 * For deletion operations (both 'elestr' and 'eleint' set to NULL) 'newp' is
 * set to the next element, on the right of the deleted one, or to NULL if the
 * deleted element was the last one. */
unsigned char *lpInsert(unsigned char *lp, unsigned char *elestr, unsigned char *eleint,
                        uint32_t size, unsigned char *p, int where, unsigned char **newp)
{
    unsigned char intenc[LP_MAX_INT_ENCODING_LEN];
    unsigned char backlen[LP_MAX_BACKLEN_SIZE];

    uint64_t enclen; /* The length of the encoded element. */
    int delete = (elestr == NULL && eleint == NULL);

    /* when deletion, it is conceptually replacing the element with a
     * zero-length element. So whatever we get passed as 'where', set
     * it to LP_REPLACE. */
    if (delete) where = LP_REPLACE;

    /* If we need to insert after the current element, we just jump to the
     * next element (that could be the EOF one) and handle the case of
     * inserting before. So the function will actually deal with just two
     * cases: LP_BEFORE and LP_REPLACE. */
    if (where == LP_AFTER) {
        p = lpSkip(p);
        where = LP_BEFORE;
        ASSERT_INTEGRITY(lp, p);
    }

    /* Store the offset of the element 'p', so that we can obtain its
     * address again after a reallocation. */
    unsigned long poff = p-lp;

    int enctype;
    if (elestr) {
        /* Calling lpEncodeGetType() results into the encoded version of the
        * element to be stored into 'intenc' in case it is representable as
        * an integer: in that case, the function returns LP_ENCODING_INT.
        * Otherwise if LP_ENCODING_STR is returned, we'll have to call
        * lpEncodeString() to actually write the encoded string on place later.
        *
        * Whatever the returned encoding is, 'enclen' is populated with the
        * length of the encoded element. */
        enctype = lpEncodeGetType(elestr,size,intenc,&enclen);
        if (enctype == LP_ENCODING_INT) eleint = intenc;
    } else if (eleint) {
        enctype = LP_ENCODING_INT;
        enclen = size; /* 'size' is the length of the encoded integer element. */
    } else {
        enctype = -1;
        enclen = 0;
    }

    /* We need to also encode the backward-parsable length of the element
     * and append it to the end: this allows to traverse the listpack from
     * the end to the start. */
    unsigned long backlen_size = (!delete) ? lpEncodeBacklen(backlen,enclen) : 0;
    uint64_t old_listpack_bytes = lpGetTotalBytes(lp);
    uint32_t replaced_len  = 0;
    if (where == LP_REPLACE) {
        replaced_len = lpCurrentEncodedSizeUnsafe(p);
        replaced_len += lpEncodeBacklen(NULL,replaced_len);
        ASSERT_INTEGRITY_LEN(lp, p, replaced_len);
    }

    uint64_t new_listpack_bytes = old_listpack_bytes + enclen + backlen_size
                                  - replaced_len;
    if (new_listpack_bytes > UINT32_MAX) return NULL;

    /* We now need to reallocate in order to make space or shrink the
     * allocation (in case 'when' value is LP_REPLACE and the new element is
     * smaller). However we do that before memmoving the memory to
     * make room for the new element if the final allocation will get
     * larger, or we do it after if the final allocation will get smaller. */

    unsigned char *dst = lp + poff; /* May be updated after reallocation. */

    /* Realloc before: we need more room. */
    if (new_listpack_bytes > old_listpack_bytes &&
        new_listpack_bytes > lp_malloc_size(lp)) {
        if ((lp = lp_realloc(lp,new_listpack_bytes)) == NULL) return NULL;
        dst = lp + poff;
    }

    /* Setup the listpack relocating the elements to make the exact room
     * we need to store the new one. */
    if (where == LP_BEFORE) {
        memmove(dst+enclen+backlen_size,dst,old_listpack_bytes-poff);
    } else { /* LP_REPLACE. */
        memmove(dst+enclen+backlen_size,
                dst+replaced_len,
                old_listpack_bytes-poff-replaced_len);
    }

    /* Realloc after: we need to free space. */
    if (new_listpack_bytes < old_listpack_bytes) {
        if ((lp = lp_realloc(lp,new_listpack_bytes)) == NULL) return NULL;
        dst = lp + poff;
    }

    /* Store the entry. */
    if (newp) {
        *newp = dst;
        /* In case of deletion, set 'newp' to NULL if the next element is
         * the EOF element. */
        if (delete && dst[0] == LP_EOF) *newp = NULL;
    }
    if (!delete) {
        if (enctype == LP_ENCODING_INT) {
            memcpy(dst,eleint,enclen);
        } else if (elestr) {
            lpEncodeString(dst,elestr,size);
        } else {
            latte_unreachable();
        }
        dst += enclen;
        memcpy(dst,backlen,backlen_size);
        dst += backlen_size;
    }

    /* Update header. */
    if (where != LP_REPLACE || delete) {
        uint32_t num_elements = lpGetNumElements(lp);
        if (num_elements != LP_HDR_NUMELE_UNKNOWN) {
            if (!delete)
                lpSetNumElements(lp,num_elements+1);
            else
                lpSetNumElements(lp,num_elements-1);
        }
    }
    lpSetTotalBytes(lp,new_listpack_bytes);

#if 0
    /* This code path is normally disabled: what it does is to force listpack
     * to return *always* a new pointer after performing some modification to
     * the listpack, even if the previous allocation was enough. This is useful
     * in order to spot bugs in code using listpacks: by doing so we can find
     * if the caller forgets to set the new pointer where the listpack reference
     * is stored, after an update. */
    unsigned char *oldlp = lp;
    lp = lp_malloc(new_listpack_bytes);
    memcpy(lp,oldlp,new_listpack_bytes);
    if (newp) {
        unsigned long offset = (*newp)-oldlp;
        *newp = lp + offset;
    }
    /* Make sure the old allocation contains garbage. */
    memset(oldlp,'A',new_listpack_bytes);
    lp_free(oldlp);
#endif

    return lp;
}

/* Append the specified element 'ele' of length 'size' at the end of the
 * listpack. It is implemented in terms of lpInsert(), so the return value is
 * the same as lpInsert(). */
unsigned char *lp_append(unsigned char *lp, unsigned char *ele, uint32_t size) {
    uint64_t listpack_bytes = lpGetTotalBytes(lp);
    unsigned char *eofptr = lp + listpack_bytes - 1;
    return lpInsert(lp,ele,NULL,size,eofptr,LP_BEFORE,NULL);
}


/* Append the specified element 's' of length 'slen' at the head of the listpack. */
unsigned char *lp_prepend(unsigned char *lp, unsigned char *s, uint32_t slen) {
    unsigned char *p = lpFirst(lp);
    if (!p) return lp_append(lp, s, slen);
    return lpInsert(lp, s, NULL, slen, p, LP_BEFORE, NULL);
}

/* Return bytes needed to encode the length of the listpack element pointed by 'p'.
 * This includes just the encoding byte, and the bytes needed to encode the length
 * of the element (excluding the element data itself)
 * If the element encoding is wrong then 0 is returned. */
static inline uint32_t lpCurrentEncodedSizeBytes(unsigned char *p) {
    if (LP_ENCODING_IS_7BIT_UINT(p[0])) return 1;
    if (LP_ENCODING_IS_6BIT_STR(p[0])) return 1;
    if (LP_ENCODING_IS_13BIT_INT(p[0])) return 1;
    if (LP_ENCODING_IS_16BIT_INT(p[0])) return 1;
    if (LP_ENCODING_IS_24BIT_INT(p[0])) return 1;
    if (LP_ENCODING_IS_32BIT_INT(p[0])) return 1;
    if (LP_ENCODING_IS_64BIT_INT(p[0])) return 1;
    if (LP_ENCODING_IS_12BIT_STR(p[0])) return 2;
    if (LP_ENCODING_IS_32BIT_STR(p[0])) return 5;
    if (p[0] == LP_EOF) return 1;
    return 0;
}

/* If 'p' points to an element of the listpack, calling lpNext() will return
 * the pointer to the next element (the one on the right), or NULL if 'p'
 * already pointed to the last element of the listpack. */
unsigned char *lpNext(unsigned char *lp, unsigned char *p) {
    assert(p);
    p = lpSkip(p);
    if (p[0] == LP_EOF) return NULL;
    lpAssertValidEntry(lp, lp_bytes(lp), p);
    return p;
}

/* Return the number of elements inside the listpack. This function attempts
 * to use the cached value when within range, otherwise a full scan is
 * needed. As a side effect of calling this function, the listpack header
 * could be modified, because if the count is found to be already within
 * the 'numele' header field range, the new value is set. */
unsigned long lpLength(unsigned char *lp) {
    uint32_t numele = lpGetNumElements(lp);
    if (numele != LP_HDR_NUMELE_UNKNOWN) return numele;

    /* Too many elements inside the listpack. We need to scan in order
     * to get the total number. */
    uint32_t count = 0;
    unsigned char *p = lpFirst(lp);
    while(p) {
        count++;
        p = lpNext(lp,p);
    }

    /* If the count is again within range of the header numele field,
     * set it. */
    if (count < LP_HDR_NUMELE_UNKNOWN) lpSetNumElements(lp,count);
    return count;
}


/* Return the listpack element pointed by 'p'.
 *
 * The function changes behavior depending on the passed 'intbuf' value.
 * Specifically, if 'intbuf' is NULL:
 *
 * If the element is internally encoded as an integer, the function returns
 * NULL and populates the integer value by reference in 'count'. Otherwise if
 * the element is encoded as a string a pointer to the string (pointing inside
 * the listpack itself) is returned, and 'count' is set to the length of the
 * string.
 *
 * If instead 'intbuf' points to a buffer passed by the caller, that must be
 * at least LP_INTBUF_SIZE bytes, the function always returns the element as
 * it was a string (returning the pointer to the string and setting the
 * 'co/* Return the listpack element pointed by 'p'.
 *
 * The function changes behavior depending on the passed 'intbuf' value.
 * Specifically, if 'intbuf' is NULL:
 *
 * If the element is internally encoded as an integer, the function returns
 * NUunt' argument to the string length by reference). However if the element
 * is encoded as an integer, the 'intbuf' buffer is used in order to store
 * the string representation.
 *
 * The user should use one or the other form depending on what the value will
 * be used for. If there is immediate usage for an integer value returned
 * by the function, than to pass a buffer (and convert it back to a number)
 * is of course useless.
 *
 * If /* Return the listpack element pointed by 'p'.
 *
 * The function changes behavior depending on the passed 'intbuf' value.
 * Specifically, if 'intbuf' is NULL:
 *
 * If the element is internally encoded as an integer, the function returns
 * NU'entry_size' is not NULL, *entry_size is set to the entry length of the
 * listpack element pointed by 'p'. This includes the encoding bytes, length
 * bytes, the element data itself, and the backlen bytes.
 *
 * If the function is called against a badly encoded ziplist, so that there
 * is no valid way to parse it, the function returns like if there was an
 * integer encoded with value 12345678900000000 + <unrecognized byte>, this may
 * be an hint to understand that something is wrong. To crash in this case is
 * not sensible because of the different requirements of the application using
 * this lib.
 *
 * Similarly, there is no error returned since the listpack normally can be
 * assumed to be valid, so that would be a very high API cost. */
static inline unsigned char *lp_get_with_size(unsigned char *p, int64_t *count, unsigned char *intbuf, uint64_t *entry_size) {
    int64_t val;
    uint64_t uval, negstart, negmax;

    assert(p); /* assertion for valgrind (avoid NPD) */
    if (LP_ENCODING_IS_7BIT_UINT(p[0])) {
        negstart = UINT64_MAX; /* 7 bit ints are always positive. */
        negmax = 0;
        uval = p[0] & 0x7f;
        if (entry_size) *entry_size = LP_ENCODING_7BIT_UINT_ENTRY_SIZE;
    } else if (LP_ENCODING_IS_6BIT_STR(p[0])) {
        *count = LP_ENCODING_6BIT_STR_LEN(p);
        if (entry_size) *entry_size = 1 + *count + lpEncodeBacklen(NULL, *count + 1);
        return p+1;
    } else if (LP_ENCODING_IS_13BIT_INT(p[0])) {
        uval = ((p[0]&0x1f)<<8) | p[1];
        negstart = (uint64_t)1<<12;
        negmax = 8191;
        if (entry_size) *entry_size = LP_ENCODING_13BIT_INT_ENTRY_SIZE;
    } else if (LP_ENCODING_IS_16BIT_INT(p[0])) {
        uval = (uint64_t)p[1] |
               (uint64_t)p[2]<<8;
        negstart = (uint64_t)1<<15;
        negmax = UINT16_MAX;
        if (entry_size) *entry_size = LP_ENCODING_16BIT_INT_ENTRY_SIZE;
    } else if (LP_ENCODING_IS_24BIT_INT(p[0])) {
        uval = (uint64_t)p[1] |
               (uint64_t)p[2]<<8 |
               (uint64_t)p[3]<<16;
        negstart = (uint64_t)1<<23;
        negmax = UINT32_MAX>>8;
        if (entry_size) *entry_size = LP_ENCODING_24BIT_INT_ENTRY_SIZE;
    } else if (LP_ENCODING_IS_32BIT_INT(p[0])) {
        uval = (uint64_t)p[1] |
               (uint64_t)p[2]<<8 |
               (uint64_t)p[3]<<16 |
               (uint64_t)p[4]<<24;
        negstart = (uint64_t)1<<31;
        negmax = UINT32_MAX;
        if (entry_size) *entry_size = LP_ENCODING_32BIT_INT_ENTRY_SIZE;
    } else if (LP_ENCODING_IS_64BIT_INT(p[0])) {
        uval = (uint64_t)p[1] |
               (uint64_t)p[2]<<8 |
               (uint64_t)p[3]<<16 |
               (uint64_t)p[4]<<24 |
               (uint64_t)p[5]<<32 |
               (uint64_t)p[6]<<40 |
               (uint64_t)p[7]<<48 |
               (uint64_t)p[8]<<56;
        negstart = (uint64_t)1<<63;
        negmax = UINT64_MAX;
        if (entry_size) *entry_size = LP_ENCODING_64BIT_INT_ENTRY_SIZE;
    } else if (LP_ENCODING_IS_12BIT_STR(p[0])) {
        *count = LP_ENCODING_12BIT_STR_LEN(p);
        if (entry_size) *entry_size = 2 + *count + lpEncodeBacklen(NULL, *count + 2);
        return p+2;
    } else if (LP_ENCODING_IS_32BIT_STR(p[0])) {
        *count = LP_ENCODING_32BIT_STR_LEN(p);
        if (entry_size) *entry_size = 5 + *count + lpEncodeBacklen(NULL, *count + 5);
        return p+5;
    } else {
        uval = 12345678900000000ULL + p[0];
        negstart = UINT64_MAX;
        negmax = 0;
    }

    /* We reach this code path only for integer encodings.
     * Convert the unsigned value to the signed one using two's complement
     * rule. */
    if (uval >= negstart) {
        /* This three steps conversion should avoid undefined behaviors
         * in the unsigned -> signed conversion. */
        uval = negmax-uval;
        val = uval;
        val = -val-1;
    } else {
        val = uval;
    }

    /* Return the string representation of the integer or the value itself
     * depending on intbuf being NULL or not. */
    if (intbuf) {
        *count = ll2string((char*)intbuf,LP_INTBUF_SIZE,(long long)val);
        return intbuf;
    } else {
        *count = val;
        return NULL;
    }
}

unsigned char *lp_get(unsigned char *p, int64_t *count, unsigned char *intbuf) {
    return lp_get_with_size(p, count, intbuf, NULL);
}

/* Print info of listpack which is used in debugCommand */
void lp_repr(unsigned char *lp) {
    unsigned char *p, *vstr;
    int64_t vlen;
    unsigned char intbuf[LP_INTBUF_SIZE];
    int index = 0;

    printf("{total bytes %zu} {num entries %lu}\n", lp_bytes(lp), lpLength(lp));

    p = lpFirst(lp);
    while(p) {
        uint32_t encoded_size_bytes = lpCurrentEncodedSizeBytes(p);
        uint32_t encoded_size = lpCurrentEncodedSizeUnsafe(p);
        unsigned long back_len = lpEncodeBacklen(NULL, encoded_size);
        printf(
                "{\n"
                "\taddr: 0x%08lx,\n"
                "\tindex: %2d,\n"
                "\toffset: %1lu,\n"
                "\thdr+entrylen+backlen: %2lu,\n"
                "\thdrlen: %3u,\n"
                "\tbacklen: %2lu,\n"
                "\tpayload: %1u\n",
                (long unsigned)p,
                index,
                (unsigned long) (p-lp),
                encoded_size + back_len,
                encoded_size_bytes,
                back_len,
                encoded_size - encoded_size_bytes);
        printf("\tbytes: ");
        for (unsigned int i = 0; i < (encoded_size + back_len); i++) {
            printf("%02x|",p[i]);
        }
        printf("\n");

        vstr = lp_get(p, &vlen, intbuf);
        printf("\t[str]");
        if (vlen > 40) {
            if (fwrite(vstr, 40, 1, stdout) == 0) perror("fwrite");
            printf("...");
        } else {
            if (fwrite(vstr, vlen, 1, stdout) == 0) perror("fwrite");
        }
        printf("\n}\n");
        index++;
        p = lpNext(lp, p);
    }
    printf("{end}\n\n");
}

/* Decode the backlen and returns it. If the encoding looks invalid (more than
 * 5 bytes are used), UINT64_MAX is returned to report the problem. */
static inline uint64_t lpDecodeBacklen(unsigned char *p) {
    uint64_t val = 0;
    uint64_t shift = 0;
    do {
        val |= (uint64_t)(p[0] & 127) << shift;
        if (!(p[0] & 128)) break;
        shift += 7;
        p--;
        if (shift > 28) return UINT64_MAX;
    } while(1);
    return val;
}

/* Validate the integrity of a single listpack entry and move to the next one.
 * The input argument 'pp' is a reference to the current record and is advanced on exit.
 * Returns 1 if valid, 0 if invalid. */
int lpValidateNext(unsigned char *lp, unsigned char **pp, size_t lpbytes) {
#define OUT_OF_RANGE(p) ( \
        (p) < lp + LP_HDR_SIZE || \
        (p) > lp + lpbytes - 1)
    unsigned char *p = *pp;
    if (!p)
        return 0;

    /* Before accessing p, make sure it's valid. */
    if (OUT_OF_RANGE(p))
        return 0;

    if (*p == LP_EOF) {
        *pp = NULL;
        return 1;
    }

    /* check that we can read the encoded size */
    uint32_t lenbytes = lpCurrentEncodedSizeBytes(p);
    if (!lenbytes)
        return 0;

    /* make sure the encoded entry length doesn't reach outside the edge of the listpack */
    if (OUT_OF_RANGE(p + lenbytes))
        return 0;

    /* get the entry length and encoded backlen. */
    unsigned long entrylen = lpCurrentEncodedSizeUnsafe(p);
    unsigned long encodedBacklen = lpEncodeBacklen(NULL,entrylen);
    entrylen += encodedBacklen;

    /* make sure the entry doesn't reach outside the edge of the listpack */
    if (OUT_OF_RANGE(p + entrylen))
        return 0;

    /* move to the next entry */
    p += entrylen;

    /* make sure the encoded length at the end patches the one at the beginning. */
    uint64_t prevlen = lpDecodeBacklen(p-1);
    if (prevlen + encodedBacklen != entrylen)
        return 0;

    *pp = p;
    return 1;
#undef OUT_OF_RANGE
}

/* Validate that the entry doesn't reach outside the listpack allocation. */
static inline void lpAssertValidEntry(unsigned char* lp, size_t lpbytes, unsigned char *p) {
    assert(lpValidateNext(lp, &p, lpbytes));
}