#include "listpack.h"
#include "zmalloc/zmalloc.h"
#include <assert.h>
#include "debug/latte_debug.h"
#include <string.h>



#define list_pack_set_total_bytes(p,v) do { \
    (p)[0] = (v)&0xff; \
    (p)[1] = ((v)>>8)&0xff; \
    (p)[2] = ((v)>>16)&0xff; \
    (p)[3] = ((v)>>24)&0xff; \
} while(0)

#define list_pack_set_num_elements(p,v) do { \
    (p)[4] = (v)&0xff; \
    (p)[5] = ((v)>>8)&0xff; \
} while(0)


#define list_pack_get_num_elements(p)          (((uint32_t)(p)[4]<<0) | \
                                      ((uint32_t)(p)[5]<<8))


#define LP_HDR_SIZE 6
#define LP_EOF 0xFF
/* 
    创建空list_pack_t* 对象
    总长度 7 = total_bytes(4)  + num_elements(2) + EOF(1)
*/
list_pack_t* list_pack_new(size_t capacity) {
    list_pack_t* lp = zmalloc(capacity > LP_HDR_SIZE + 1? capacity: LP_HDR_SIZE+ 1);
    if (lp == NULL) return NULL;
    list_pack_set_total_bytes(lp, LP_HDR_SIZE + 1); /* 设置长度为7  */
    list_pack_set_num_elements(lp, 0); /*设置个数 0 */
    lp[LP_HDR_SIZE] = LP_EOF;
    return lp;
}

void list_pack_delete(list_pack_t* lp) {
    zfree(lp);
}

void list_pack_free_generic(void* lp) {
    list_pack_delete((list_pack_t*)lp);
}


#define list_pack_get_total_bytes(p)           (((uint32_t)(p)[0]<<0) | \
                                      ((uint32_t)(p)[1]<<8) | \
                                      ((uint32_t)(p)[2]<<16) | \
                                      ((uint32_t)(p)[3]<<24))

/*内存收缩*/
unsigned char* list_pack_shrink_to_fit(list_pack_t* lp) {
    size_t size = list_pack_get_total_bytes(lp);
    if (size < zmalloc_size(lp)) {
        return zrealloc(lp, size);
    } else {
        return lp;
    }
}


#define LP_MAX_INT_ENCODING_LEN 9
#define LP_MAX_BACKLEN_SIZE 5

#define assert_integrity(lp, p) do { \
    latte_assert((p) >= (lp)+LP_HDR_SIZE && (p) < (lp)+list_pack_get_total_bytes((lp))); \
} while (0)

#define assert_integrity_len(lp, p, len) do { \
    assert((p) >= (lp)+LP_HDR_SIZE && (p)+(len) < (lp)+list_pack_get_total_bytes((lp))); \
} while (0)

#define LP_ENCODING_INT 0 /*数字类型*/
#define LP_ENCODING_STRING 1 /*字符串类型*/



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

#define LP_ENCODING_6BIT_STR_LEN(p) ((p)[0] & 0x3F)
#define LP_ENCODING_12BIT_STR_LEN(p) ((((p)[0] & 0xF) << 8) | (p)[1])
#define LP_ENCODING_32BIT_STR_LEN(p) (((uint32_t)(p)[1]<<0) | \
                                      ((uint32_t)(p)[2]<<8) | \
                                      ((uint32_t)(p)[3]<<16) | \
                                      ((uint32_t)(p)[4]<<24))


static inline unsigned long list_pack_encode_backlen_bytes(uint64_t l) {
    if (l <= 127) {
        return 1;
    } else if (l < 16383) {
        return 2;
    } else if (l < 2097151) {
        return 3;
    } else if (l < 268435455) {
        return 4;
    } else {
        return 5;
    }
}
static inline uint32_t list_pack_current_encode_size_unsafe(unsigned char *p) {
    if (LP_ENCODING_IS_7BIT_UINT(p[0])) return 1;
    if (LP_ENCODING_IS_6BIT_STR(p[0])) return 1+LP_ENCODING_6BIT_STR_LEN(p); /*标记开头2位 10 短字符串*/
    if (LP_ENCODING_IS_13BIT_INT(p[0])) return 2;   /* 前3位 110 */
    if (LP_ENCODING_IS_16BIT_INT(p[0])) return 3;   /* 11100001 */
    if (LP_ENCODING_IS_24BIT_INT(p[0])) return 4;   /* 11110010 */
    if (LP_ENCODING_IS_32BIT_INT(p[0])) return 5;   /* 11110011 */
    if (LP_ENCODING_IS_64BIT_INT(p[0])) return 9;   /* 11110100 */
    if (LP_ENCODING_IS_12BIT_STR(p[0])) return 2+LP_ENCODING_12BIT_STR_LEN(p); /* 前8位 11100000 */
    if (LP_ENCODING_IS_32BIT_STR(p[0])) return 5+LP_ENCODING_32BIT_STR_LEN(p); /* 前8位 11110000 */
    if (p[0] == LP_EOF) return 1;
    return 0;
}

static inline unsigned char *list_pack_skip(unsigned char *p) {
    unsigned long entrylen = list_pack_current_encode_size_unsafe(p);
    entrylen += list_pack_encode_backlen_bytes(entrylen);
    p += entrylen;
    return p;
}

static inline void list_pack_encode_integer_get_type(int64_t v, unsigned char *intenc, uint64_t *enclen) {
    if (v >= 0 && v <= 127) {
        /* Single byte 0-127 integer. */
        if (intenc != NULL) intenc[0] = v;
        if (enclen != NULL) *enclen = 1;
    } else if (v >= -4096 && v <= 4095) {
        /* 13 bit integer. */
        if (v < 0) v = ((int64_t)1<<13)+v;
        if (intenc != NULL) {
            intenc[0] = (v>>8)|LP_ENCODING_13BIT_INT;
            intenc[1] = v&0xff;
        }
        if (enclen != NULL) *enclen = 2;
    } else if (v >= -32768 && v <= 32767) {
        /* 16 bit integer. */
        if (v < 0) v = ((int64_t)1<<16)+v;
        if (intenc != NULL) {
            intenc[0] = LP_ENCODING_16BIT_INT;
            intenc[1] = v&0xff;
            intenc[2] = v>>8;
        }
        if (enclen != NULL) *enclen = 3;
    } else if (v >= -8388608 && v <= 8388607) {
        /* 24 bit integer. */
        if (v < 0) v = ((int64_t)1<<24)+v;
        if (intenc != NULL) {
            intenc[0] = LP_ENCODING_24BIT_INT;
            intenc[1] = v&0xff;
            intenc[2] = (v>>8)&0xff;
            intenc[3] = v>>16;
        }
        if (enclen != NULL) *enclen = 4;
    } else if (v >= -2147483648 && v <= 2147483647) {
        /* 32 bit integer. */
        if (v < 0) v = ((int64_t)1<<32)+v;
        if (intenc != NULL) {
            intenc[0] = LP_ENCODING_32BIT_INT;
            intenc[1] = v&0xff;
            intenc[2] = (v>>8)&0xff;
            intenc[3] = (v>>16)&0xff;
            intenc[4] = v>>24;
        }
        if (enclen != NULL) *enclen = 5;
    } else {
        /* 64 bit integer. */
        uint64_t uv = v;
        if (intenc != NULL) {
            intenc[0] = LP_ENCODING_64BIT_INT;
            intenc[1] = uv&0xff;
            intenc[2] = (uv>>8)&0xff;
            intenc[3] = (uv>>16)&0xff;
            intenc[4] = (uv>>24)&0xff;
            intenc[5] = (uv>>32)&0xff;
            intenc[6] = (uv>>40)&0xff;
            intenc[7] = (uv>>48)&0xff;
            intenc[8] = uv>>56;
        }
        if (enclen != NULL) *enclen = 9;
    }
}
#define LONG_STR_SIZE  21
int list_pack_string_to_int64(const char *s, unsigned long slen, int64_t *value) {
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

static inline int list_pack_encode_get_type(unsigned char *ele, uint32_t size, unsigned char *intenc, uint64_t *enclen) {
    int64_t v;
    if (list_pack_string_to_int64((const char*)ele, size, &v)) {
        list_pack_encode_integer_get_type(v, intenc, enclen);
        return LP_ENCODING_INT;
    } else {
        if (size < 64) *enclen = 1+size;
        else if (size < 4096) *enclen = 2+size;
        else *enclen = 5+(uint64_t)size;
        return LP_ENCODING_STRING;
    }
}

static inline unsigned long list_pack_encode_backlen(unsigned char *buf, uint64_t l) {
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

static inline void list_pack_encode_string(unsigned char *buf, unsigned char *s, uint32_t len) {
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

list_pack_t* list_pack_insert(list_pack_t* lp, unsigned char* elestr, unsigned char* eleint, uint32_t size, 
    unsigned char* p, int where, unsigned char** newp) {
    unsigned char intenc[LP_MAX_INT_ENCODING_LEN];
    unsigned char backlen[LP_MAX_BACKLEN_SIZE];
    uint64_t enclen;
    int delete = (elestr == NULL && eleint == NULL); /* 判断是删除还是插入 */

    if (delete) where = LP_REPLACE;
    if (where == LP_AFTER) {
        p = list_pack_skip(p);
        where = LP_BEFORE;
        assert_integrity(lp, p);
    }

    unsigned long poff = p - lp;

    int enctype;
    if (elestr) {   /*插入字符串*/
        enctype = list_pack_encode_get_type(elestr, size, intenc, &enclen);
        if (enctype == LP_ENCODING_INT) eleint = intenc;
    } else if (eleint) { /*插入数字*/
        enctype =  LP_ENCODING_INT; 
        enclen = size;
    } else {
        enctype = -1; /*删除行为*/
        enclen = 0;;
    }

    
    unsigned long backlen_size = (!delete) ? list_pack_encode_backlen(backlen,enclen) : 0; /*解析出长度*/
    uint64_t old_list_pack_bytes = list_pack_get_total_bytes(lp);
    uint32_t replaced_len = 0;
    if (where == LP_REPLACE) {
        replaced_len = list_pack_current_encode_size_unsafe(p);
        replaced_len += list_pack_encode_backlen_bytes(replaced_len);
        assert_integrity_len(lp, p, replaced_len);
    }
    uint64_t new_list_pack_bytes = old_list_pack_bytes + size + backlen_size
                             - replaced_len;/*当前lp长度  +  (类型 + 数据长度) + （backlen长度） - 删除长度*/
    if (new_list_pack_bytes > UINT32_MAX) return NULL; /* 超过32位最大值*/

    unsigned char* dst = lp + poff;
    if (new_list_pack_bytes > old_list_pack_bytes 
        && new_list_pack_bytes > zmalloc_size(lp)) { /* 如果新长度大于旧长度，并且大于当前内存大小，则重新分配内存 */
       if ((lp = zrealloc(lp, new_list_pack_bytes)) == NULL) return NULL;
       dst = lp + poff;/* 重新分配内存后，需要更新dst指针 */
    }
    
    if (where == LP_BEFORE) {/* 插入 */
        memmove(dst + enclen + backlen_size, dst, 
            old_list_pack_bytes - poff); /* 插入后，需要将后面的数据移动到后面 */
    } else { /* 删除 */
        memmove(dst + enclen + backlen_size, 
            dst + replaced_len, 
            old_list_pack_bytes - poff - replaced_len); /* 删除后，需要将后面的数据移动到前面 */
    }

    if (new_list_pack_bytes < old_list_pack_bytes) { /* 如果新长度小于旧长度，则重新分配内存 */
        if ((lp = zrealloc(lp, new_list_pack_bytes)) == NULL) return NULL;
        dst = lp + poff; /* 重新分配内存后，需要更新dst指针 */
    }

    if (newp) {
        *newp = dst; /*设置添加点*/
        if (delete && dst[0] == LP_EOF) *newp = NULL; /* 如果删除后，lp为空，则设置添加点为NULL */
    }

    if (!delete) { /* 插入 */
        if (enctype == LP_ENCODING_INT) { /* 插入数字 */
            memcpy(dst, eleint, enclen);
        } else if(elestr) { /* 插入字符串 */
            list_pack_encode_string(dst, elestr, size);
        } else { /* 异常 */
            latte_assert(0);
        }
        dst += enclen; /* 移动到插入backlen的位置 */
        memcpy(dst, backlen, backlen_size); /* 插入backlen */
        dst += backlen_size; /* 移动到结束位置 */
    }

    if (where != LP_REPLACE || delete) {
        uint32_t num_elements = list_pack_get_num_elements(lp);
        if (num_elements != UINT16_MAX) { /* 如果个数不是最大值，则更新个数 */
            if (!delete) 
                list_pack_set_num_elements(lp, num_elements + 1); /* 插入 个数+1 */
            else 
                list_pack_set_num_elements(lp, num_elements - 1); /* 删除 个数-1 */
        }
    }
    list_pack_set_total_bytes(lp, new_list_pack_bytes); /* 设置新的长度 */
/*  如果是debug 模式的话 可以开始检测数据是否正确 */
    return lp;
}

/* 插入字符串 */
unsigned char* list_pack_insert_string(list_pack_t* lp, unsigned char* s, uint32_t slen, 
    unsigned char* p, int where, list_pack_t** newp) {
    return list_pack_insert(lp, s, NULL, slen, p, where, newp);
}