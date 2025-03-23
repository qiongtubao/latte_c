
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include "sds.h"

const char *SDS_NOINIT = "SDS_NOINIT";
static inline int sdsHdrSize(char type) {
    switch(type&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            return sizeof(struct sdshdr5);
        case SDS_TYPE_8:
            return sizeof(struct sdshdr8);
        case SDS_TYPE_16:
            return sizeof(struct sdshdr16);
        case SDS_TYPE_32:
            return sizeof(struct sdshdr32);
        case SDS_TYPE_64:
            return sizeof(struct sdshdr64);
    }
    return 0;
}

static inline char sdsReqType(size_t string_size) {
    if (string_size < 1<<5)
        return SDS_TYPE_5;
    if (string_size < 1<<8)
        return SDS_TYPE_8;
    if (string_size < 1<<16)
        return SDS_TYPE_16;
#if (LONG_MAX == LLONG_MAX)
    if (string_size < 1ll<<32)
        return SDS_TYPE_32;
#endif
    return SDS_TYPE_64;
}

static inline size_t sdsTypeMaxSize(char type) {
    if (type == SDS_TYPE_5)
        return (1<<5) - 1;
    if (type == SDS_TYPE_8)
        return (1<<8) - 1;
    if (type == SDS_TYPE_16)
        return (1<<16) - 1;
#if (LONG_MAX == LLONG_MAX)
    if (type == SDS_TYPE_32)
        return (1ll<<32) - 1;
#endif
    return -1; /* this is equivalent to the max SDS_TYPE_64 or SDS_TYPE_32 */
}

sds_t _sds_new_len(const void *init, size_t initlen, int trymalloc) {
    void *sh;
    sds_t s;
    //根据初始化的长度判断使用什么类型
    char type = sdsReqType(initlen); 
    //如果创建空字符串后追加数据的话 使用type8的 更适合
    if (type == SDS_TYPE_5 && initlen == 0) type = SDS_TYPE_8;
    //获得类型所用的长度
    int hdrlen = sdsHdrSize(type);
    unsigned char *fp; /* flags pointer. */
    size_t usable;
    assert(initlen + hdrlen + 1 > initlen); /* Catch size_t overflow */
    //前缀 + 内容 + \0
    sh = trymalloc?
        s_trymalloc_usable(hdrlen+initlen+1, &usable) :
        s_malloc_usable(hdrlen+initlen+1, &usable);
    if (sh == NULL) return NULL;
    if (init==SDS_NOINIT)
        init = NULL;
    else if (!init)
        memset(sh, 0, hdrlen+initlen+1);
    s = (char*)sh+hdrlen;
    fp = ((unsigned char*)s)-1;
    usable = usable-hdrlen-1;
    if (usable > sdsTypeMaxSize(type))
        usable = sdsTypeMaxSize(type);
    switch(type) {
        case SDS_TYPE_5: {
            *fp = type | (initlen << SDS_TYPE_BITS);
            break;
        }
        case SDS_TYPE_8: {
            SDS_HDR_VAR(8,s);
            sh->len = initlen;
            sh->alloc = usable;
            *fp = type;
            break;
        }
        case SDS_TYPE_16: {
            SDS_HDR_VAR(16,s);
            sh->len = initlen;
            sh->alloc = usable;
            *fp = type;
            break;
        }
        case SDS_TYPE_32: {
            SDS_HDR_VAR(32,s);
            sh->len = initlen;
            sh->alloc = usable;
            *fp = type;
            break;
        }
        case SDS_TYPE_64: {
            SDS_HDR_VAR(64,s);
            sh->len = initlen;
            sh->alloc = usable;
            *fp = type;
            break;
        }
    }
    if (initlen && init)
        memcpy(s, init, initlen);
    s[initlen] = '\0';
    return s;
}

sds_t sds_new_len(const void *init, size_t initlen) {
    return _sds_new_len(init, initlen, 0);
}

sds_t sds_try_new_len(const void *init, size_t initlen) {
    return _sds_new_len(init, initlen, 1);
}


/**
 * @brief 
 *      Create a new sds_t string starting from a null terminated C string
 * @param init 
 * @param initlen 
 * @return sds
 * @example 
 *       sds_t x = sds_new_len("foo", 2);
 *       assert(sds_len(x) == 2);
 *       assert(memcmp(x, "fo\0", 3) == 0);
 * 
 */
sds_t sds_new(const char *init) {
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return sds_new_len(init, initlen);
}


/* Free an sds_t string. No operation is performed if 's' is NULL. */
void sds_delete(sds_t s) {
    if (s == NULL) return;
    s_free((char*)s-sdsHdrSize(s[-1]));
}


/* Enlarge the free space at the end of the sds_t string so that the caller
 * is sure that after calling this function can overwrite up to addlen
 * bytes after the end of the string, plus one more byte for nul term.
 *
 * Note: this does not change the *length* of the sds_t string as returned
 * by sds_len(), but only the free buffer space we have. */
#define SDS_MAKE_ROOM_EXACT (1<<0)
static sds_t _sds_make_room_for(sds_t s, size_t addlen, int flags) {
    void *sh, *newsh;
    size_t avail = sds_avail(s);
    size_t len, newlen, reqlen;
    char type, oldtype = s[-1] & SDS_TYPE_MASK;
    int hdrlen;
    size_t usable;

    /* Return ASAP if there is enough space left. */
    if (avail >= addlen) return s;

    len = sds_len(s);
    sh = (char*)s-sdsHdrSize(oldtype);
    reqlen = newlen = (len+addlen);
    assert(newlen > len);   /* Catch size_t overflow */
    if (!(flags&SDS_MAKE_ROOM_EXACT)) {
        if (newlen < SDS_MAX_PREALLOC)
            newlen *= 2;
        else
            newlen += SDS_MAX_PREALLOC;
    }

    type = sdsReqType(newlen);

    /* Don't use type 5: the user is appending to the string and type 5 is
     * not able to remember empty space, so sds_make_room_for() must be called
     * at every appending operation. */
    if (type == SDS_TYPE_5) type = SDS_TYPE_8;

    hdrlen = sdsHdrSize(type);
    assert(hdrlen + newlen + 1 > reqlen);  /* Catch size_t overflow */
    if (oldtype==type) {
        newsh = s_realloc_usable(sh, hdrlen+newlen+1, &usable);
        if (newsh == NULL) return NULL;
        s = (char*)newsh+hdrlen;
    } else {
        /* Since the header size changes, need to move the string forward,
         * and can't use realloc */
        newsh = s_malloc_usable(hdrlen+newlen+1, &usable);
        if (newsh == NULL) return NULL;
        memcpy((char*)newsh+hdrlen, s, len+1);
        s_free(sh);
        s = (char*)newsh+hdrlen;
        s[-1] = type;
        sds_set_len(s, len);
    }
    usable = usable-hdrlen-1;
    if (usable > sdsTypeMaxSize(type))
        usable = sdsTypeMaxSize(type);
    sds_set_alloc(s, usable);
    return s;
}

sds_t sds_make_room_for(sds_t s, size_t addlen) {
    return _sds_make_room_for(s,addlen,0);
}

sds_t sds_make_room_forExact(sds_t s, size_t addlen) {
    return _sds_make_room_for(s,addlen,SDS_MAKE_ROOM_EXACT);
}

sds_t sds_cat_len(sds_t s, const void *t, size_t len) {
    size_t curlen = sds_len(s);

    s = sds_make_room_for(s,len);
    if (s == NULL) return NULL;
    memcpy(s+curlen, t, len);
    sds_set_len(s, curlen+len);
    s[curlen+len] = '\0';
    return s;
}

sds_t sds_cat(sds_t s, const char *t) {
    return sds_cat_len(s, t, strlen(t));
}

sds_t sds_cat_sds(sds_t s, const sds_t t) {
    return sds_cat_len(s, t, sds_len(t));
}



/* Changes the input string to be a subset of the original.
 * It does not release the free space in the string, so a call to
 * sdsRemoveFreeSpace may be wise after. */
void sdssubstr(sds_t s, size_t start, size_t len) {
    /* Clamp out of range input */
    size_t oldlen = sds_len(s);
    if (start >= oldlen) start = len = 0;
    if (len > oldlen-start) len = oldlen-start;

    /* Move the data */
    if (len) memmove(s, s+start, len);
    s[len] = 0;
    sds_set_len(s,len);
}
/* Turn the string into a smaller (or equal) string containing only the
 * substring specified by the 'start' and 'end' indexes.
 *
 * start and end can be negative, where -1 means the last character of the
 * string, -2 the penultimate character, and so forth.
 *
 * The interval is inclusive, so the start and end characters will be part
 * of the resulting string.
 *
 * The string is modified in-place.
 *
 * NOTE: this function can be misleading and can have unexpected behaviour,
 * specifically when you want the length of the new string to be 0.
 * Having start==end will result in a string with one character.
 * please consider using sdssubstr instead.
 *
 * Example:
 *
 * s = sds_new("Hello World");
 * sds_range(s,1,-1); => "ello World"
 */
void sds_range(sds_t s, ssize_t start, ssize_t end) {
    size_t newlen, len = sds_len(s);
    if (len == 0) return;
    if (start < 0)
        start = len + start;
    if (end < 0)
        end = len + end;
    newlen = (start > end) ? 0 : (end-start)+1;
    sdssubstr(s, start, newlen);
}

/* Remove the part of the string from left and from right composed just of
 * contiguous characters found in 'cset', that is a null terminted C string.
 *
 * After the call, the modified sds_t string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = sds_new("AA...AA.a.aa.aHelloWorld     :::");
 * s = sds_trim(s,"Aa. :");
 * printf("%s\n", s);
 *
 * Output will be just "HelloWorld".
 */
sds_t sds_trim(sds_t s, const char *cset) {
    char *start, *end, *sp, *ep;
    size_t len;

    sp = start = s;
    ep = end = s+sds_len(s)-1;
    while(sp <= end && strchr(cset, *sp)) sp++;
    while(ep > sp && strchr(cset, *ep)) ep--;
    len = (sp > ep) ? 0 : ((ep-sp)+1);
    if (s != sp) memmove(s, sp, len);
    s[len] = '\0';
    sds_set_len(s,len);
    return s;
}

/* Create an empty (zero length) sds_t string. Even in this case the string
 * always has an implicit null term. */
sds_t sds_empty(void) {
    return sds_new_len("",0);
}

/**
 * 创建一个长度为length的空字符串
 * 提前创建大的buffer
 */
sds_t sds_empty_len(int length) {
    sds_t result = sds_new_len(NULL, length);
    sds_set_len(result, 0);
    return result;
}



/* Like sds_cat_printf() but gets va_list instead of being variadic. */
sds_t sds_catvprintf(sds_t s, const char *fmt, va_list ap) {
    va_list cpy;
    char staticbuf[1024], *buf = staticbuf, *t;
    size_t buflen = strlen(fmt)*2;
    int bufstrlen;

    /* We try to start using a static buffer for speed.
     * If not possible we revert to heap allocation. */
    if (buflen > sizeof(staticbuf)) {
        buf = s_malloc(buflen);
        if (buf == NULL) return NULL;
    } else {
        buflen = sizeof(staticbuf);
    }

    /* Alloc enough space for buffer and \0 after failing to
     * fit the string in the current buffer size. */
    while(1) {
        va_copy(cpy,ap);
        bufstrlen = vsnprintf(buf, buflen, fmt, cpy);
        va_end(cpy);
        if (bufstrlen < 0) {
            if (buf != staticbuf) s_free(buf);
            return NULL;
        }
        if (((size_t)bufstrlen) >= buflen) {
            if (buf != staticbuf) s_free(buf);
            buflen = ((size_t)bufstrlen) + 1;
            buf = s_malloc(buflen);
            if (buf == NULL) return NULL;
            continue;
        }
        break;
    }

    /* Finally concat the obtained string to the SDS string and return it. */
    t = sds_cat_len(s, buf, bufstrlen);
    if (buf != staticbuf) s_free(buf);
    return t;
}

/* Append to the sds_t string 's' a string obtained using printf-alike format
 * specifier.
 *
 * After the call, the modified sds_t string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = sds_new("Sum is: ");
 * s = sds_cat_printf(s,"%d+%d = %d",a,b,a+b).
 *
 * Often you need to create a string from scratch with the printf-alike
 * format. When this is the need, just use sds_empty() as the target string:
 *
 * s = sds_cat_printf(sds_empty(), "... your format ...", args);
 */
sds_t sds_cat_printf(sds_t s, const char *fmt, ...) {
    va_list ap;
    char *t;
    va_start(ap, fmt);
    t = sds_catvprintf(s,fmt,ap);
    va_end(ap);
    return t;
}

/* Append to the sds_t string "s" an escaped string representation where
 * all the non-printable characters (tested with isprint()) are turned into
 * escapes in the form "\n\r\a...." or "\x<hex-number>".
 *
 * After the call, the modified sds_t string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
sds_t sds_catrepr(sds_t s, const char *p, size_t len) {
    s = sds_cat_len(s,"\"",1);
    while(len--) {
        switch(*p) {
        case '\\':
        case '"':
            s = sds_cat_printf(s,"\\%c",*p);
            break;
        case '\n': s = sds_cat_len(s,"\\n",2); break;
        case '\r': s = sds_cat_len(s,"\\r",2); break;
        case '\t': s = sds_cat_len(s,"\\t",2); break;
        case '\a': s = sds_cat_len(s,"\\a",2); break;
        case '\b': s = sds_cat_len(s,"\\b",2); break;
        default:
            if (isprint(*p))
                s = sds_cat_printf(s,"%c",*p);
            else
                s = sds_cat_printf(s,"\\x%02x",(unsigned char)*p);
            break;
        }
        p++;
    }
    return sds_cat_len(s,"\"",1);
}

/* Helper function for sds_split_args() that returns non zero if 'c'
 * is a valid hex digit. */
int is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

/* Helper function for sds_split_args() that converts a hex digit into an
 * integer from 0 to 15 */
int hex_digit_to_int(char c) {
    switch(c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': case 'A': return 10;
    case 'b': case 'B': return 11;
    case 'c': case 'C': return 12;
    case 'd': case 'D': return 13;
    case 'e': case 'E': return 14;
    case 'f': case 'F': return 15;
    default: return 0;
    }
}


/* Split a line into arguments, where every argument can be in the
 * following programming-language REPL-alike form:
 *
 * foo bar "newline are supported\n" and "\xff\x00otherstuff"
 *
 * The number of arguments is stored into *argc, and an array
 * of sds_t is returned.
 *
 * The caller should free the resulting array of sds_t strings with
 * sds_free_splitres().
 *
 * Note that sds_catrepr() is able to convert back a string into
 * a quoted string in the same format sds_split_args() is able to parse.
 *
 * The function returns the allocated tokens on success, even when the
 * input string is empty, or NULL if the input contains unbalanced
 * quotes or closed quotes followed by non space characters
 * as in: "foo"bar or "foo'
 */
sds_t *sds_split_args(const char *line, int *argc) {
    const char *p = line;
    char *current = NULL;
    char **vector = NULL;

    *argc = 0;
    while(1) {
        /* skip blanks */
        while(*p && isspace(*p)) p++;
        if (*p) {
            /* get a token */
            int inq=0;  /* set to 1 if we are in "quotes" */
            int insq=0; /* set to 1 if we are in 'single quotes' */
            int done=0;

            if (current == NULL) current = sds_empty();
            while(!done) {
                if (inq) {
                    if (*p == '\\' && *(p+1) == 'x' &&
                                             is_hex_digit(*(p+2)) &&
                                             is_hex_digit(*(p+3)))
                    {
                        unsigned char byte;

                        byte = (hex_digit_to_int(*(p+2))*16)+
                                hex_digit_to_int(*(p+3));
                        current = sds_cat_len(current,(char*)&byte,1);
                        p += 3;
                    } else if (*p == '\\' && *(p+1)) {
                        char c;

                        p++;
                        switch(*p) {
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                        case 't': c = '\t'; break;
                        case 'b': c = '\b'; break;
                        case 'a': c = '\a'; break;
                        default: c = *p; break;
                        }
                        current = sds_cat_len(current,&c,1);
                    } else if (*p == '"') {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (*(p+1) && !isspace(*(p+1))) goto err;
                        done=1;
                    } else if (!*p) {
                        /* unterminated quotes */
                        goto err;
                    } else {
                        current = sds_cat_len(current,p,1);
                    }
                } else if (insq) {
                    if (*p == '\\' && *(p+1) == '\'') {
                        p++;
                        current = sds_cat_len(current,"'",1);
                    } else if (*p == '\'') {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (*(p+1) && !isspace(*(p+1))) goto err;
                        done=1;
                    } else if (!*p) {
                        /* unterminated quotes */
                        goto err;
                    } else {
                        current = sds_cat_len(current,p,1);
                    }
                } else {
                    switch(*p) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                    case '\0':
                        done=1;
                        break;
                    case '"':
                        inq=1;
                        break;
                    case '\'':
                        insq=1;
                        break;
                    default:
                        current = sds_cat_len(current,p,1);
                        break;
                    }
                }
                if (*p) p++;
            }
            /* add the token to the vector */
            vector = s_realloc(vector,((*argc)+1)*sizeof(char*));
            vector[*argc] = current;
            (*argc)++;
            current = NULL;
        } else {
            /* Even on empty input string return something not NULL. */
            if (vector == NULL) vector = s_malloc(sizeof(void*));
            return vector;
        }
    }

err:
    while((*argc)--)
        sds_delete(vector[*argc]);
    s_free(vector);
    if (current) sds_delete(current);
    *argc = 0;
    return NULL;
}

/* Free the result returned by sds_split_len(), or do nothing if 'tokens' is NULL. */
void sds_free_splitres(sds_t *tokens, int count) {
    if (!tokens) return;
    while(count--)
        sds_delete(tokens[count]);
    s_free(tokens);
}

/* Split 's' with separator in 'sep'. An array
 * of sds_t strings is returned. *count will be set
 * by reference to the number of tokens returned.
 *
 * On out of memory, zero length string, zero length
 * separator, NULL is returned.
 *
 * Note that 'sep' is able to split a string using
 * a multi-character separator. For example
 * sdssplit("foo_-_bar","_-_"); will return two
 * elements "foo" and "bar".
 *
 * This version of the function is binary-safe but
 * requires length arguments. sdssplit() is just the
 * same function but for zero-terminated strings.
 */
sds_t *sds_split_len(const char *s, ssize_t len, const char *sep, int seplen, int *count) {
    int elements = 0, slots = 5;
    long start = 0, j;
    sds_t *tokens;

    if (seplen < 1 || len < 0) return NULL;

    tokens = s_malloc(sizeof(sds)*slots);
    if (tokens == NULL) return NULL;

    if (len == 0) {
        *count = 0;
        return tokens;
    }
    for (j = 0; j < (len-(seplen-1)); j++) {
        /* make sure there is room for the next element and the final one */
        if (slots < elements+2) {
            sds_t *newtokens;

            slots *= 2;
            newtokens = s_realloc(tokens,sizeof(sds)*slots);
            if (newtokens == NULL) goto cleanup;
            tokens = newtokens;
        }
        /* search the separator */
        if ((seplen == 1 && *(s+j) == sep[0]) || (memcmp(s+j,sep,seplen) == 0)) {
            tokens[elements] = sds_new_len(s+start,j-start);
            if (tokens[elements] == NULL) goto cleanup;
            elements++;
            start = j+seplen;
            j = j+seplen-1; /* skip the separator */
        }
    }
    /* Add the final element. We are sure there is room in the tokens array. */
    tokens[elements] = sds_new_len(s+start,len-start);
    if (tokens[elements] == NULL) goto cleanup;
    elements++;
    *count = elements;
    return tokens;

cleanup:
    {
        int i;
        for (i = 0; i < elements; i++) sds_delete(tokens[i]);
        s_free(tokens);
        *count = 0;
        return NULL;
    }
}

/* Compare two sds_t strings s1 and s2 with memcmp().
 *
 * Return value:
 *
 *     positive if s1 > s2.
 *     negative if s1 < s2.
 *     0 if s1 and s2 are exactly the same binary string.
 *
 * If two strings share exactly the same prefix, but one of the two has
 * additional characters, the longer string is considered to be greater than
 * the smaller one. */
int sds_cmp(const sds_t s1, const sds_t s2) {
    size_t l1, l2, minlen;
    int cmp;

    l1 = sds_len(s1);
    l2 = sds_len(s2);
    minlen = (l1 < l2) ? l1 : l2;
    cmp = memcmp(s1,s2,minlen);
    if (cmp == 0) return l1>l2? 1: (l1<l2? -1: 0);
    return cmp;
}

/* 
 * 
 * 增加sds的长度，并根据'incr'减少字符串末尾的剩余空闲空间。
 * 同时在字符串的新末尾设置空终止符。
 * 
 * 此函数用于在用户调用sds_make_room_for()后，
 * 在当前字符串的末尾写入内容，
 * 并最终需要设置新的长度时修复字符串的长度。
 * 注意：可以使用负增量来右修剪字符串。
 * 使用示例：
 * 使用sds_incr_len()和sds_make_room_for()可以实现以下方案，将来自内核的字节连接到sds字符串的末尾，而无需复制到中间缓冲区：

 * oldlen = sds_len(s);
 * s = sds_make_room_for(s, BUFFER_SIZE);
 * nread = read(fd, s+oldlen, BUFFER_SIZE);
 * ... 检查nread <= 0并处理 ...
 * sds_incr_len(s, nread);
 */
void sds_incr_len(sds_t s, ssize_t incr) {
    unsigned char flags = s[-1];
    size_t len;
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5: {
            unsigned char *fp = ((unsigned char*)s)-1;
            unsigned char oldlen = SDS_TYPE_5_LEN(flags);
            assert((incr > 0 && oldlen+incr < 32) || (incr < 0 && oldlen >= (unsigned int)(-incr)));
            *fp = SDS_TYPE_5 | ((oldlen+incr) << SDS_TYPE_BITS);
            len = oldlen+incr;
            break;
        }
        case SDS_TYPE_8: {
            SDS_HDR_VAR(8,s);
            assert((incr >= 0 && sh->alloc-sh->len >= incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
            len = (sh->len += incr);
            break;
        }
        case SDS_TYPE_16: {
            SDS_HDR_VAR(16,s);
            assert((incr >= 0 && sh->alloc-sh->len >= incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
            len = (sh->len += incr);
            break;
        }
        case SDS_TYPE_32: {
            SDS_HDR_VAR(32,s);
            assert((incr >= 0 && sh->alloc-sh->len >= (unsigned int)incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
            len = (sh->len += incr);
            break;
        }
        case SDS_TYPE_64: {
            SDS_HDR_VAR(64,s);
            assert((incr >= 0 && sh->alloc-sh->len >= (uint64_t)incr) || (incr < 0 && sh->len >= (uint64_t)(-incr)));
            len = (sh->len += incr);
            break;
        }
        default: len = 0; /* Just to avoid compilation warnings. */
    }
    s[len] = '\0';
}

/* Duplicate an sds_t string. */
sds_t sds_dup(const sds_t s) {
    return sds_new_len(s, sds_len(s));
}

/* 调整分配大小，这可以使分配更大或更小，
* 如果大小小于当前使用的长度，数据将被截断。
*
* 当 would_regrow 参数设置为 1 时，它会阻止使用
* SDS_TYPE_5，当 sds_t 可能再次更改时，这是所需的。
*
* 无论实际分配大小如何，sdsAlloc 大小都将设置为请求的大小，这样做是为了避免在调用者检测到它有多余的空间时重复调用此
* 函数。*/
sds_t sds_resize(sds_t s, size_t size, int would_regrow) {
    void *sh, *newsh;
    char type, oldtype = s[-1] & SDS_TYPE_MASK;
    int hdrlen, oldhdrlen = sdsHdrSize(oldtype);
    size_t len = sds_len(s);
    sh = (char*)s-oldhdrlen;

    /* Return ASAP if the size is already good. */
    if (sds_alloc(s) == size) return s;

    /* Truncate len if needed. */
    if (size < len) len = size;

    /* Check what would be the minimum SDS header that is just good enough to
     * fit this string. */
    type = sdsReqType(size);
    if (would_regrow) {
        /* Don't use type 5, it is not good for strings that are expected to grow back. */
        if (type == SDS_TYPE_5) type = SDS_TYPE_8;
    }
    hdrlen = sdsHdrSize(type);

    /* If the type is the same, or can hold the size in it with low overhead
     * (larger than SDS_TYPE_8), we just realloc(), letting the allocator
     * to do the copy only if really needed. Otherwise if the change is
     * huge, we manually reallocate the string to use the different header
     * type. */
    int use_realloc = (oldtype==type || (type < oldtype && type > SDS_TYPE_8));
    size_t newlen = use_realloc ? oldhdrlen+size+1 : hdrlen+size+1;

    if (use_realloc) {
        int alloc_already_optimal = 0;
        #if defined(USE_JEMALLOC)
            /* je_nallocx returns the expected allocation size for the newlen.
             * We aim to avoid calling realloc() when using Jemalloc if there is no
             * change in the allocation size, as it incurs a cost even if the
             * allocation size stays the same. */
            alloc_already_optimal = (je_nallocx(newlen, 0) == zmalloc_size(sh));
        #endif
        if (!alloc_already_optimal) {
            newsh = s_realloc(sh, newlen);
            if (newsh == NULL) return NULL;
            s = (char*)newsh+oldhdrlen;
        }
    } else {
        newsh = s_malloc(newlen);
        if (newsh == NULL) return NULL;
        memcpy((char*)newsh+hdrlen, s, len);
        s_free(sh);
        s = (char*)newsh+hdrlen;
        s[-1] = type;
    }
    s[len] = 0;
    sds_set_len(s, len);
    sds_set_alloc(s, size);
    return s;
}

//给fs 提供路径判断的函数  后续再优化 更好用

//c++ rfind函数
// 用于查找子字符串最后一次出现位置的函数
size_t sds_find_lastof(sds_t haystack, const char *needle) {
    size_t needle_len = strlen(needle);
    size_t haystack_len = strlen(haystack);

    if (needle_len == 0) {
        return 0; // 如果needle为空，返回0
    }

    if (needle_len > haystack_len) {
        return (size_t)-1; // 如果needle比haystack长，返回-1
    }

    for (size_t i = haystack_len - needle_len; i != (size_t)-1; i--) {
        if (strncmp(haystack + i, needle, needle_len) == 0) {
            return i; // 找到了needle，返回其在haystack中的位置
        }
    }

    return C_NPOS; // 没有找到needle，返回-1
}

//c++ starts_with函数
int sds_starts_with(sds_t str, const char *prefix) {
    size_t str_len = sds_len(str);
    size_t prefix_len = strlen(prefix);

    // 如果前缀的长度大于字符串的长度，显然不可能以这个前缀开头
    if (prefix_len > str_len) {
        return 0; // 或者可以返回 false，取决于你的真假值表示
    }

    // 使用strncmp比较字符串的前缀部分和给定的前缀是否相等
    if (strncmp(str, prefix, prefix_len) == 0) {
        return 1; // 或者返回 true
    }

    return 0; // 或者返回 false
}

/* This function is similar to sds_cat_printf, but much faster as it does
 * not rely on sprintf() family functions implemented by the libc that
 * are often very slow. Moreover directly handling the sds_t string as
 * new data is concatenated provides a performance improvement.
 *
 * However this function only handles an incompatible subset of printf-alike
 * format specifiers:
 *
 * %s - C String
 * %S - SDS string
 * %i - signed int
 * %I - 64 bit signed integer (long long, int64_t)
 * %u - unsigned int
 * %U - 64 bit unsigned integer (unsigned long long, uint64_t)
 * %% - Verbatim "%" character.
 */
#define SDS_LLSTR_SIZE 21
int sdsll2str(char *s, long long value) {
    char *p, aux;
    unsigned long long v;
    size_t l;

    /* Generate the string representation, this method produces
     * a reversed string. */
    v = (value < 0) ? -value : value;
    p = s;
    do {
        *p++ = '0'+(v%10);
        v /= 10;
    } while(v);
    if (value < 0) *p++ = '-';

    /* Compute length and add null term. */
    l = p-s;
    *p = '\0';

    /* Reverse the string. */
    p--;
    while(s < p) {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}

/* Identical sdsll2str(), but for unsigned long long type. */
int sdsull2str(char *s, unsigned long long v) {
    char *p, aux;
    size_t l;

    /* Generate the string representation, this method produces
     * a reversed string. */
    p = s;
    do {
        *p++ = '0'+(v%10);
        v /= 10;
    } while(v);

    /* Compute length and add null term. */
    l = p-s;
    *p = '\0';

    /* Reverse the string. */
    p--;
    while(s < p) {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}

/* This function is similar to sds_cat_printf, but much faster as it does
 * not rely on sprintf() family functions implemented by the libc that
 * are often very slow. Moreover directly handling the sds_t string as
 * new data is concatenated provides a performance improvement.
 *
 * However this function only handles an incompatible subset of printf-alike
 * format specifiers:
 *
 * %s - C String
 * %S - SDS string
 * %i - signed int
 * %I - 64 bit signed integer (long long, int64_t)
 * %u - unsigned int
 * %U - 64 bit unsigned integer (unsigned long long, uint64_t)
 * %% - Verbatim "%" character.
 */
sds_t sds_cat_fmt(sds_t s, char const *fmt, ...) {
    size_t initlen = sds_len(s);
    const char *f = fmt;
    long i;
    va_list ap;

    /* To avoid continuous reallocations, let's start with a buffer that
     * can hold at least two times the format string itself. It's not the
     * best heuristic but seems to work in practice. */
    s = sds_make_room_for(s, strlen(fmt)*2);
    va_start(ap,fmt);
    f = fmt;    /* Next format specifier byte to process. */
    i = initlen; /* Position of the next byte to write to dest str. */
    while(*f) {
        char next, *str;
        size_t l;
        long long num;
        unsigned long long unum;

        /* Make sure there is always space for at least 1 char. */
        if (sds_avail(s)==0) {
            s = sds_make_room_for(s,1);
        }

        switch(*f) {
        case '%':
            next = *(f+1);
            f++;
            switch(next) {
            case 's':
            case 'S':
                str = va_arg(ap,char*);
                l = (next == 's') ? strlen(str) : sds_len(str);
                if (sds_avail(s) < l) {
                    s = sds_make_room_for(s,l);
                }
                memcpy(s+i,str,l);
                sds_inc_len(s,l);
                i += l;
                break;
            case 'i':
            case 'I':
                if (next == 'i')
                    num = va_arg(ap,int);
                else
                    num = va_arg(ap,long long);
                {
                    char buf[SDS_LLSTR_SIZE];
                    l = sdsll2str(buf,num);
                    if (sds_avail(s) < l) {
                        s = sds_make_room_for(s,l);
                    }
                    memcpy(s+i,buf,l);
                    sds_inc_len(s,l);
                    i += l;
                }
                break;
            case 'u':
            case 'U':
                if (next == 'u')
                    unum = va_arg(ap,unsigned int);
                else
                    unum = va_arg(ap,unsigned long long);
                {
                    char buf[SDS_LLSTR_SIZE];
                    l = sdsull2str(buf,unum);
                    if (sds_avail(s) < l) {
                        s = sds_make_room_for(s,l);
                    }
                    memcpy(s+i,buf,l);
                    sds_inc_len(s,l);
                    i += l;
                }
                break;
            default: /* Handle %% and generally %<unknown>. */
                s[i++] = next;
                sds_inc_len(s,1);
                break;
            }
            break;
        default:
            s[i++] = *f;
            sds_inc_len(s,1);
            break;
        }
        f++;
    }
    va_end(ap);

    /* Add null-term */
    s[i] = '\0';
    return s;
}

sds_t sds_reset(sds_t s, char* data, int len) {
    sds_clear(s);
    return sds_cat_len(s, data, len);
}

/* Modify an sds_t string in-place to make it empty (zero length).
 * However all the existing buffer is not discarded but set as free space
 * so that next append operations will not require allocations up to the
 * number of bytes previously available. */
void sds_clear(sds_t s) {
    sds_set_len(s, 0);
    s[0] = '\0';
}



/* Like sdscatprintf() but gets va_list instead of being variadic. */
sds sds_cat_vprintf(sds s, const char *fmt, va_list ap) {
    va_list cpy;
    char staticbuf[1024], *buf = staticbuf, *t;
    size_t buflen = strlen(fmt)*2;
    int bufstrlen;

    /* We try to start using a static buffer for speed.
     * If not possible we revert to heap allocation. */
    if (buflen > sizeof(staticbuf)) {
        buf = s_malloc(buflen);
        if (buf == NULL) return NULL;
    } else {
        buflen = sizeof(staticbuf);
    }

    /* Alloc enough space for buffer and \0 after failing to
     * fit the string in the current buffer size. */
    while(1) {
        va_copy(cpy,ap);
        bufstrlen = vsnprintf(buf, buflen, fmt, cpy);
        va_end(cpy);
        if (bufstrlen < 0) {
            if (buf != staticbuf) s_free(buf);
            return NULL;
        }
        if (((size_t)bufstrlen) >= buflen) {
            if (buf != staticbuf) s_free(buf);
            buflen = ((size_t)bufstrlen) + 1;
            buf = s_malloc(buflen);
            if (buf == NULL) return NULL;
            continue;
        }
        break;
    }

    /* Finally concat the obtained string to the SDS string and return it. */
    t = sds_cat_len(s, buf, bufstrlen);
    if (buf != staticbuf) s_free(buf);
    return t;
}