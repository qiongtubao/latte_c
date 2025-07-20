
#include "string.h"
#include "sds/sds.h"
#include "utils/utils.h"
#include "utils/utils.h"
#include <string.h>
#include "debug/latte_debug.h"

#define OBJ_SHARED_INTEGERS 10000

latte_object_t *embedded_string_object_new(const char *ptr, size_t len) {
    latte_object_t*  obj = zmalloc(sizeof(latte_object_t) + sizeof(struct sdshdr8)+len + 1);
    struct sdshdr8 *sh = (void*)(obj + 1);
    obj->type = OBJ_STRING;
    obj->encoding = OBJ_ENCODING_EMBSTR;
    obj->ptr = sh + 1;
    obj->refcount = 1;
    sh->len = len;
    sh->alloc = len;
    sh->flags = SDS_TYPE_8;
    if (ptr == SDS_NOINIT) 
        sh->buf[len] = '\0';
    else if (ptr) {
        memcpy(sh->buf, ptr, len);
        sh->buf[len] = '\0';
    } else {
        memset(sh->buf, 0, len + 1);
    }
    return obj;

}


/* Create a string object with encoding OBJ_ENCODING_RAW, that is a plain
 * string object where o->ptr points to a proper sds string. */
latte_object_t *raw_string_object_new(const char *ptr, size_t len) {
    return latte_object_new(OBJ_STRING, sds_new_len(ptr,len));
}

/* Create a string object with EMBSTR encoding if it is smaller than
 * OBJ_ENCODING_EMBSTR_SIZE_LIMIT, otherwise the RAW encoding is
 * used.
 *
 * The current limit of 44 is chosen so that the biggest string object
 * we allocate as EMBSTR will still fit into the 64 byte arena of jemalloc. */
#define OBJ_ENCODING_EMBSTR_SIZE_LIMIT 44
latte_object_t* string_object_new(const char* ptr, size_t len) {
    if (len <= OBJ_ENCODING_EMBSTR_SIZE_LIMIT) {
        return embedded_string_object_new(ptr, len);
    } else {
        return raw_string_object_new(ptr, len);
    }
}

latte_object_t *try_raw_string_object_new(const char* ptr, size_t len) {
    sds str = sds_try_new_len(ptr,len);
    if (!str) return NULL;
    return latte_object_new(OBJ_STRING, str);
}

latte_object_t *try_string_object_new(const char* ptr, size_t len) {
    if (len <= OBJ_ENCODING_EMBSTR_SIZE_LIMIT) {
        return embedded_string_object_new(ptr, len);
    } else {
        return try_raw_string_object_new(ptr, len);
    }
}

void init_shared_integers() {
    static bool inited = false;
    if (inited == true) return;
    for (int j = 0; j < OBJ_SHARED_INTEGERS; j++) {
        shared_integers.integers[j] =
            make_object_shared(latte_object_new(OBJ_STRING,(void*)(long)j));
        shared_integers.integers[j]->encoding = OBJ_ENCODING_INT;
    }
    inited = true;
}

latte_object_t *string_object_from_long_long_with_options_new(long long value, int valueobj) {
    latte_object_t *o;

    // if (server.maxmemory == 0 ||
    //     !(server.maxmemory_policy & MAXMEMORY_FLAG_NO_SHARED_INTEGERS))
    // {
    //     /* If the maxmemory policy permits, we can still return shared integers
    //      * even if valueobj is true. */
    //     valueobj = 0;
    // }

    if (value >= 0 && value < OBJ_SHARED_INTEGERS && valueobj == 0) {
        init_shared_integers();
        latte_object_incr_ref_count(shared_integers.integers[value]);
        o = shared_integers.integers[value];
    } else {
        if (value >= LONG_MIN && value <= LONG_MAX) {
            o = latte_object_new(OBJ_STRING, NULL);
            o->encoding = OBJ_ENCODING_INT;
            o->ptr = (void*)((long)value);
        } else {
            o = latte_object_new(OBJ_STRING,sds_from_longlong(value));
        }
    }
    return o;
}

latte_object_t *string_object_from_long_long(long long value) {
    return string_object_from_long_long_with_options_new(value, 0);
}

latte_object_t *string_object_from_long_long_for_value(long long value) {
    return string_object_from_long_long_with_options_new(value, 1);
}


latte_object_t *string_object_from_long_double(long double value, int humanfriendly) {
    char buf[MAX_LONG_DOUBLE_CHARS];
     int len = ld2string(buf,sizeof(buf),value,humanfriendly? LD_STR_HUMAN: LD_STR_AUTO);
    return string_object_new(buf, len);
}

latte_object_t *dup_string_object(const latte_object_t* o) {
    latte_object_t* d;

    switch(d->encoding) {
        case OBJ_ENCODING_RAW:
            return raw_string_object_new(d->ptr, sds_len(d->ptr));
        case OBJ_ENCODING_EMBSTR:
            return embedded_string_object_new(o->ptr, sds_len(o->ptr));
        case OBJ_ENCODING_INT:
            d = latte_object_new(OBJ_STRING, NULL);
            d->encoding = OBJ_ENCODING_INT;
            d->ptr = o->ptr;
            return d;
        default:
            latte_panic("Wrong encoding.");
            break;
    }
}

void trim_string_object_if_needed(latte_object_t* o) {
    if (o->encoding == OBJ_ENCODING_RAW && sds_avail(o->ptr) > sds_len(o->ptr)/10) {
        o->ptr = sds_remove_free_space(o->ptr, 0);
    }
}

#define REDIS_COMPARE_BINARY (1<<0)
#define REDIS_COMPARE_COLL (1<<1)
#define sds_encoded_object(objptr) (objptr->encoding == OBJ_ENCODING_RAW || objptr->encoding == OBJ_ENCODING_EMBSTR)
int compare_string_objects_with_flags(latte_object_t* a, latte_object_t* b, int flags) {
    char bufa[128], bufb[128], *astr, *bstr;
    size_t alen, blen, minlen;

    if (a == b) return 0;
    if (sds_encoded_object(a)) {
        astr = a->ptr;
        alen = sds_len(astr);
    } else {
        alen = ll2string(bufa, sizeof(bufa), (long) a->ptr);
        astr = bufa;
    } 
    if (sds_encoded_object(b)) {
        bstr = b->ptr;
        blen = sds_len(bstr);
    }

    if (flags & REDIS_COMPARE_COLL) {
        return strcoll(astr,bstr);
    } else {
        int cmp;

        minlen = (alen < blen) ? alen : blen;
        cmp = memcmp(astr,bstr,minlen);
        if (cmp == 0) return alen-blen;
        return cmp;
    }
}

int compare_string_objects(latte_object_t* a, latte_object_t* b) {
    return compare_string_objects_with_flags(a, b, REDIS_COMPARE_BINARY);
}

int collate_string_objects(latte_object_t* a, latte_object_t* b) {
    return compare_string_objects_with_flags(a, b, REDIS_COMPARE_COLL);
}

int equal_string_objects(latte_object_t* a, latte_object_t* b) {
    if (a->encoding == OBJ_ENCODING_INT && 
        b->encoding == OBJ_ENCODING_INT) {
            return a->ptr == b->ptr;
    } else {
        return compare_string_objects(a, b) == 0;
    }
}

size_t string_object_len(latte_object_t* o) {
    if (sds_encoded_object(o)) {
        return sds_len(o->ptr);
    } else {
        return sdigits10((long)o->ptr);
    }
}

int get_double_from_object(const latte_object_t* o, double *target) {
    double value;
    if (o == NULL) {
        value = 0;
    } else {
        if (sds_encoded_object(o)) {
            if (!string2d(o->ptr, sds_len(o->ptr), &value))
                return -1;
        } else if (o ->encoding == OBJ_ENCODING_INT) {
            value = (long)o->ptr;
        } else {

        }
    }
    *target = value;
    return 0;
}

int get_long_double_from_object(latte_object_t* o, long double *target) {
    long double value;
    if (o == NULL) {
        value = 0;
    } else {
        
        if (sds_encoded_object(o)) {
            if (!string2ld(o->ptr, sds_len(o->ptr), &value))
                return -1;
        } else if (o->encoding == OBJ_ENCODING_INT) {
            value = (long)o->ptr;
        } else {
            latte_panic("Unknown string encoding");
        }
    }
    *target = value;
    return 0;
}

int get_long_long_from_object(latte_object_t* o, long long *target) {
    long long value;

    if (o == NULL) {
        value = 0;
    } else {
        if (sds_encoded_object(o)) {
            if (string2ll(o->ptr, sds_len(o->ptr), &value) == 0) return -1;
        } else if (o->encoding == OBJ_ENCODING_INT) {
            value = (long)o->ptr;
        } else {
            latte_panic("Unknown string encoding");
        }
    }
    if (target) *target = value;
    return 0;
}

int get_sds_from_object(latte_object_t* o, sds *target) {
    if (o->type != OBJ_STRING) {
        return -1;
    }
    *target = o->ptr;
    return 0;
}

/* Convert a string into a long. Returns 1 if the string could be parsed into a
 * (non-overflowing) long, 0 otherwise. The value will be set to the parsed
 * value when appropriate. */
int string2l(const char *s, size_t slen, long *lval) {
    long long llval;

    if (!string2ll(s,slen,&llval))
        return 0;

    if (llval < LONG_MIN || llval > LONG_MAX)
        return 0;

    *lval = (long)llval;
    return 1;
}

latte_object_t* try_object_encoding(latte_object_t* o) {
    long value;
    sds_t s = o->ptr;
    size_t len ;

    if (!sds_encoded_object(o)) return o;

    if (o->refcount > 1) return o;

    len = sds_len(s);

    if (len <= 20 && string2l(s, len, &value)) {
        if ((value >= 0 && value < OBJ_SHARED_INTEGERS)) {
            latte_object_decr_ref_count(o);
            latte_object_incr_ref_count(shared_integers.integers[value]);
            return shared_integers.integers[value];
        } else {
            if (o->encoding == OBJ_ENCODING_RAW) {
                sds_delete(o->ptr);
                o->encoding = OBJ_ENCODING_INT;
                o->ptr= (void*) value;
                return o;
            }  else if (o->encoding == OBJ_ENCODING_EMBSTR) {
                latte_object_decr_ref_count(o);
                return string_object_from_long_long_for_value(value);
            }
        }
    }

    if (len <= OBJ_ENCODING_EMBSTR_SIZE_LIMIT) {
        latte_object_t* emb;

        if (o->encoding == OBJ_ENCODING_EMBSTR) return o;
        emb = embedded_string_object_new(s, sds_len(s));
        latte_object_decr_ref_count(o);
        return emb;
    }

    trim_string_object_if_needed(o);
    return o;
}

latte_object_t* get_decode_object(latte_object_t* o) {
    latte_object_t* dec;

    if (sds_encoded_object(o)) {
        latte_object_incr_ref_count(o);
        return o;
    }
    if (o->type == OBJ_STRING && o->encoding == OBJ_ENCODING_INT) {
        char buf[32];
        ll2string(buf, 32, (long)o->ptr);
        dec = string_object_new(buf, strlen(buf));
        return dec;
    } else {
        latte_panic("Unknown encoding type");
    }
}

size_t object_string_compute_size(latte_object_t* o, size_t sample_size) {
    size_t asize = 0;
    if (o->encoding == OBJ_ENCODING_INT) {
        asize = sizeof(*o);
    } else if (o->encoding == OBJ_ENCODING_RAW) {
        asize = sds_zmalloc_size(o->ptr)+sizeof(*o);
    } else if (o->encoding == OBJ_ENCODING_EMBSTR ) {
        asize = sds_len(o->ptr) + 2+ sizeof(*o);
    } else {
        latte_panic("Unknown string encoding");
    }
    return asize;
}

void free_string_object(latte_object_t *o) {
    if (o->encoding == OBJ_ENCODING_RAW) {
        sds_delete(o->ptr);
    }
}   