
#ifndef __LATTE_SDS_H
#define __LATTE_SDS_H
#define SDS_MAX_PREALLOC (1024*1024)
#include <sys/types.h>
#include <stdarg.h>
#include <stdint.h>
#include "zmalloc/zmalloc.h"

#define s_malloc zmalloc
#define s_realloc zrealloc
#define s_realloc_usable zrealloc_usable
#define s_free zfree
#define s_malloc_usable zmalloc_usable
#define s_trymalloc_usable ztrymalloc_usable

typedef char *sds;
#define sds_t sds
extern const char *SDS_NOINIT;

struct __attribute__ ((__packed__)) sdshdr5 {
    unsigned char flags; /* 3 lsb of type, and 5 msb of string length */
    char buf[];
};

struct __attribute__ ((__packed__)) sdshdr8 {
    uint8_t len; /* used */
    uint8_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__ ((__packed__)) sdshdr16 {
    uint16_t len; /* used */
    uint16_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__ ((__packed__)) sdshdr32 {
    uint32_t len; /* used */
    uint32_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__ ((__packed__)) sdshdr64 {
    uint64_t len; /* used */
    uint64_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};

#define SDS_TYPE_5  0
#define SDS_TYPE_8  1
#define SDS_TYPE_16 2
#define SDS_TYPE_32 3
#define SDS_TYPE_64 4
#define SDS_TYPE_MASK 7
#define SDS_TYPE_BITS 3
#define SDS_HDR_VAR(T,s) struct sdshdr##T *sh = (struct sdshdr##T*)((s)-(sizeof(struct sdshdr##T)));
#define SDS_HDR(T,s) ((struct sdshdr##T *)((s)-(sizeof(struct sdshdr##T))))
#define SDS_TYPE_5_LEN(f) ((f)>>SDS_TYPE_BITS)


static inline size_t sds_len(const sds_t s) {
    unsigned char flags = s[-1];
    switch (flags & SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            return SDS_TYPE_5_LEN(flags);
        case SDS_TYPE_8:
            return SDS_HDR(8,s)->len;
        case SDS_TYPE_16:
            return SDS_HDR(16,s)->len;
        case SDS_TYPE_32:
            return SDS_HDR(32,s)->len;
        case SDS_TYPE_64:
            return SDS_HDR(64,s)->len;
    }
    return 0;
}

static inline void sds_set_len(sds_t s, size_t newlen) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
        {
            unsigned char *fp = ((unsigned char*)s)-1;
            *fp = SDS_TYPE_5 | (newlen << SDS_TYPE_BITS);
        }
            break;
        case SDS_TYPE_8:
            SDS_HDR(8,s)->len = newlen;
            break;
        case SDS_TYPE_16:
            SDS_HDR(16,s)->len = newlen;
            break;
        case SDS_TYPE_32:
            SDS_HDR(32,s)->len = newlen;
            break;
        case SDS_TYPE_64:
            SDS_HDR(64,s)->len = newlen;
            break;
    }
}

static inline void sds_set_alloc(sds_t s, size_t newlen) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            /* Nothing to do, this type has no total allocation info. */
            break;
        case SDS_TYPE_8:
            SDS_HDR(8,s)->alloc = newlen;
            break;
        case SDS_TYPE_16:
            SDS_HDR(16,s)->alloc = newlen;
            break;
        case SDS_TYPE_32:
            SDS_HDR(32,s)->alloc = newlen;
            break;
        case SDS_TYPE_64:
            SDS_HDR(64,s)->alloc = newlen;
            break;
    }
}

static inline size_t sds_avail(const sds_t s) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5: {
            return 0;
        }
        case SDS_TYPE_8: {
            SDS_HDR_VAR(8,s);
            return sh->alloc - sh->len;
        }
        case SDS_TYPE_16: {
            SDS_HDR_VAR(16,s);
            return sh->alloc - sh->len;
        }
        case SDS_TYPE_32: {
            SDS_HDR_VAR(32,s);
            return sh->alloc - sh->len;
        }
        case SDS_TYPE_64: {
            SDS_HDR_VAR(64,s);
            return sh->alloc - sh->len;
        }
    }
    return 0;
}
/* sds_alloc() = sds_avail() + sds_len() */
static inline size_t sds_alloc(const sds_t s) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            return SDS_TYPE_5_LEN(flags);
        case SDS_TYPE_8:
            return SDS_HDR(8,s)->alloc;
        case SDS_TYPE_16:
            return SDS_HDR(16,s)->alloc;
        case SDS_TYPE_32:
            return SDS_HDR(32,s)->alloc;
        case SDS_TYPE_64:
            return SDS_HDR(64,s)->alloc;
    }
    return 0;
}

static inline void sds_inc_len(sds_t s, size_t inc) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
        {
            unsigned char *fp = ((unsigned char*)s)-1;
            unsigned char newlen = SDS_TYPE_5_LEN(flags)+inc;
            *fp = SDS_TYPE_5 | (newlen << SDS_TYPE_BITS);
        }
            break;
        case SDS_TYPE_8:
            SDS_HDR(8,s)->len += inc;
            break;
        case SDS_TYPE_16:
            SDS_HDR(16,s)->len += inc;
            break;
        case SDS_TYPE_32:
            SDS_HDR(32,s)->len += inc;
            break;
        case SDS_TYPE_64:
            SDS_HDR(64,s)->len += inc;
            break;
    }
}
/**
 *
 * @param init
 * @return
 * @example
 *      sds_t s = sds_new("foo");
 *      assert(strcmp(s, "foo") == 0);
 */
sds_t sds_new(const char *init);
void sds_delete(sds_t s);
/**
 *
 * @param init
 * @param initlen
 * @return
 * @example
 *     sds_t x = sds_new("foo");
 *     assert(sds_len(x) == 3);
 *     assert(memcmp(x, "foo\0", 4) == 0);
 */
sds_t sds_new_len(const void *init, size_t initlen);
sds_t sds_try_new_len(const void *init, size_t initlen);

/**
 *
 * @return
 * @example
 *    sds_t empty = sds_empty();
 *    assert(sds_len(empty) == 0);
 */
sds_t sds_empty(void);
sds_t sds_empty_len(int length);
/**
 *
 * @param s
 * @param t
 * @return
 * @example
 *      sds_t x = sds_new_len("foo", 2);
 *      x = sds_cat(x, "bar");
 *      assert(sds_len(x) == 5);
 *      assert(memcmp(x, "fobar\0", 6) == 0);
 */
sds_t sds_cat(sds_t s, const char *t);

/**
 *
 * @param s
 * @param fmt
 * @param ...
 * @return
 * @example
 *      sds_t x = sds_empty();
 *      x = sds_cat_fmt(x, "string:%s", "hello");
 *      assert(memcmp(x, "string:hello\0", strlen("string:hello\0")) == 0);
 *      sds_delete(x);
 *      x = sds_empty();
 *      x = sds_cat_fmt(x, "int:%i", 100);
 *      assert(memcmp(x, "int:100\0", strlen("int:100\0")) == 0);
 *      sds_delete(x);
 *      x = sds_empty();
 *      x = sds_cat_fmt(x, "long:%u", 2147483648);
 *      assert(memcmp(x, "long:2147483648\0", strlen("long:2147483648\0")) == 0);
 */
sds_t sds_cat_fmt(sds_t s, char const *fmt, ...);

/**
 * @brief 
 * 
 * @param s 
 * @param t 
 * @param len 
 * @return sds
 * @example
 *      sds_t x = sds_empty();
 *      x = sds_cat_len(x, "helloabcded", 5);
 *      assert(memcpy(x, "hello\0",6));
 */
sds_t sds_cat_len(sds_t s, const void *t, size_t len);

/**
 *
 * @param s
 * @param len
 * @param sep
 * @param seplen
 * @param count
 * @return
 * @example
 *      int len = 0;
 *      sds* splits = sds_split_len("test_a_b_c", 10, "_", 1, &len);
 *      assert(len == 4);
 *      assert(memcmp(splits[0], "test\0",5) == 0);
 *      assert(memcmp(splits[1], "a\0",2) == 0);
 *      assert(memcmp(splits[2], "b\0",2) == 0);
 *      assert(memcmp(splits[3], "c\0",2) == 0);
 *      sds_free_splitres(splits, len);
 */
void sds_free_splitres(sds_t *tokens, int count);

/**
 *
 * @param s1
 * @param s2
 * @return
 * @example
 *      sds_t x = sds_new("hello");
 *      sds_t y = sds_new("hello");
 *      assert(sds_cmp(sds_cmp(x, y) == 0);
 */
int sds_cmp(const sds_t s1, const sds_t s2);

sds_t sds_cat_printf(sds_t s, const char *fmt, ...);

/**
 *
 * @param value
 * @return
 * @example
 *      sds_t ll = sds_from_longlong(100);
 *      assert(memcmp(ll, "100\0", 4) == 0);
 */
sds_t sds_from_longlong(long long value);

/**
 *
 * @param s
 * @param t
 * @return
 * @example
 *      sds_t x = sds_new("x");
 *      sds_t y = sds_new("y");
 *      x = sds_cat_sds(x, y);
 *      assert(memcmp(x, "xy\0", 3) == 0);
 */
sds_t sds_cat_sds(sds_t s, const sds_t t);
/**
 *
 * @param s
 * @param cset
 * @return
 * @example
 *      s = sds_new("AA...AA.a.aa.aHelloWorld     :::");
 *      s = sds_trim(s,"Aa. :");
 *      assert(memcmp(s, "HelloWorld\0", 11) == 0);
 */
sds_t sds_trim(sds_t s, const char *cset);

sds_t sds_catrepr(sds_t s, const char *p, size_t len);
sds_t *sds_split_args(const char *line, int *argc);
sds_t *sds_split_len(const char *s, ssize_t len, const char *sep, int seplen, int *count);
// void sdstolower(sds_t s);

/**
 * @brief 
 * 
 * @param s
 * @example 
 *   sds_t x = sds_new("hello");
 *   assert(sds_len(x) == 5);
 *   sds_clear(x);
 *   assert(sds_len(x) == 0);
 *  
 */
void sds_clear(sds_t s);


void sds_range(sds_t s, ssize_t start, ssize_t end);
void sds_incr_len(sds_t s, ssize_t incr);
sds_t sds_make_room_for(sds_t s, size_t addlen);
sds_t sds_dup(const sds_t s);
sds_t sds_resize(sds_t s, size_t size, int would_regrow);
#define C_NPOS ((size_t)(-1))
size_t sds_find_lastof(sds_t haystack, const char *needle);
int sds_starts_with(sds_t str, const char *prefix);
sds_t sds_reset(sds_t s, char* data, int len);
sds sds_cat_vprintf(sds s, const char *fmt, va_list ap);
sds sds_map_chars(sds s, const char *from, const char *to, size_t setlen);
sds sds_remove_free_space(sds s, int would_regrow);
size_t sds_zmalloc_size(sds s);
#endif
