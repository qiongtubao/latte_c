

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


static inline size_t sdslen(const sds s) {
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

static inline void sdssetlen(sds s, size_t newlen) {
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

static inline void sdssetalloc(sds s, size_t newlen) {
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

static inline size_t sdsavail(const sds s) {
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
/* sdsalloc() = sdsavail() + sdslen() */
static inline size_t sdsalloc(const sds s) {
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

static inline void sdsinclen(sds s, size_t inc) {
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
 *      sds s = sdsnew("foo");
 *      assert(strcmp(s, "foo") == 0);
 */
sds sdsnew(const char *init);
void sdsfree(sds s);
/**
 *
 * @param init
 * @param initlen
 * @return
 * @example
 *     sds x = sdsnew("foo");
 *     assert(sdslen(x) == 3);
 *     assert(memcmp(x, "foo\0", 4) == 0);
 */
sds sdsnewlen(const void *init, size_t initlen);
sds sdstrynewlen(const void *init, size_t initlen);

/**
 *
 * @return
 * @example
 *    sds empty = sdsempty();
 *    assert(sdslen(empty) == 0);
 */
sds sdsempty(void);
/**
 *
 * @param s
 * @param t
 * @return
 * @example
 *      sds x = sdsnewlen("foo", 2);
 *      x = sdscat(x, "bar");
 *      assert(sdslen(x) == 5);
 *      assert(memcmp(x, "fobar\0", 6) == 0);
 */
sds sdscat(sds s, const char *t);

/**
 *
 * @param s
 * @param fmt
 * @param ...
 * @return
 * @example
 *      sds x = sdsempty();
 *      x = sdscatfmt(x, "string:%s", "hello");
 *      assert(memcmp(x, "string:hello\0", strlen("string:hello\0")) == 0);
 *      sdsfree(x);
 *      x = sdsempty();
 *      x = sdscatfmt(x, "int:%i", 100);
 *      assert(memcmp(x, "int:100\0", strlen("int:100\0")) == 0);
 *      sdsfree(x);
 *      x = sdsempty();
 *      x = sdscatfmt(x, "long:%u", 2147483648);
 *      assert(memcmp(x, "long:2147483648\0", strlen("long:2147483648\0")) == 0);
 */
sds sdscatfmt(sds s, char const *fmt, ...);

/**
 * @brief 
 * 
 * @param s 
 * @param t 
 * @param len 
 * @return sds
 * @example
 *      sds x = sdsempty();
 *      x = sdscatlen(x, "helloabcded", 5);
 *      assert(memcpy(x, "hello\0",6));
 */
sds sdscatlen(sds s, const void *t, size_t len);

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
 *      sds* splits = sdssplitlen("test_a_b_c", 10, "_", 1, &len);
 *      assert(len == 4);
 *      assert(memcmp(splits[0], "test\0",5) == 0);
 *      assert(memcmp(splits[1], "a\0",2) == 0);
 *      assert(memcmp(splits[2], "b\0",2) == 0);
 *      assert(memcmp(splits[3], "c\0",2) == 0);
 *      sdsfreesplitres(splits, len);
 */
void sdsfreesplitres(sds *tokens, int count);

/**
 *
 * @param s1
 * @param s2
 * @return
 * @example
 *      sds x = sdsnew("hello");
 *      sds y = sdsnew("hello");
 *      assert(sdscmp(sdscmp(x, y) == 0);
 */
int sdscmp(const sds s1, const sds s2);

sds sdscatprintf(sds s, const char *fmt, ...);

/**
 *
 * @param value
 * @return
 * @example
 *      sds ll = sdsfromlonglong(100);
 *      assert(memcmp(ll, "100\0", 4) == 0);
 */
sds sdsfromlonglong(long long value);

/**
 *
 * @param s
 * @param t
 * @return
 * @example
 *      sds x = sdsnew("x");
 *      sds y = sdsnew("y");
 *      x = sdscatsds(x, y);
 *      assert(memcmp(x, "xy\0", 3) == 0);
 */
sds sdscatsds(sds s, const sds t);
/**
 *
 * @param s
 * @param cset
 * @return
 * @example
 *      s = sdsnew("AA...AA.a.aa.aHelloWorld     :::");
 *      s = sdstrim(s,"Aa. :");
 *      assert(memcmp(s, "HelloWorld\0", 11) == 0);
 */
sds sdstrim(sds s, const char *cset);

sds sdscatrepr(sds s, const char *p, size_t len);
sds *sdssplitargs(const char *line, int *argc);
void sdsfreesplitres(sds *tokens, int count);
sds *sdssplitlen(const char *s, ssize_t len, const char *sep, int seplen, int *count);
// void sdstolower(sds s);

/**
 * @brief 
 * 
 * @param s
 * @example 
 *   sds x = sdsnew("hello");
 *   assert(sdslen(x) == 5);
 *   sdsclear(x);
 *   assert(sdslen(x) == 0);
 *  
 */
void sdsclear(sds s);


void sdsrange(sds s, ssize_t start, ssize_t end);
