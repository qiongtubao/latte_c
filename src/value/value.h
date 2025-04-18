#ifndef __LATTE_VALUE_H
#define __LATTE_VALUE_H

#include "sds/sds.h"
#include "vector/vector.h"
#include <stdbool.h>
#include "dict/dict.h"


typedef enum value_type_enum {
    VALUE_UNDEFINED,
    VALUE_CONSTANT_CHAR,
    VALUE_SDS,
    VALUE_INT,
    VALUE_UINT,
    VALUE_DOUBLE,
    VALUE_BOOLEAN,
    VALUE_ARRAY,
    VALUE_MAP,
} value_type_enum;
typedef struct value_t {
    value_type_enum type;
    union {
        sds_t sds_value;
        int64_t i64_value;
        uint64_t  u64_value;
        long double ld_value;
        bool bool_value;
        vector_t* array_value;
        dict_t* map_value;
    } value;
} value_t;

value_t* value_new();
void value_delete(value_t* v);
void value_clean(value_t* v);


sds_t value_get_sds(value_t* v);
int64_t value_get_int64(value_t* v);
uint64_t value_get_uint64(value_t* v);
long double value_get_long_double(value_t* v);
bool value_get_bool(value_t* v);
vector_t* value_get_array(value_t* v);
dict_t* value_get_map(value_t* v);
char* value_get_constant_char(value_t* v);

sds_t value_get_binary(value_t* v);

void value_set_sds(value_t* v, sds_t s);
void value_set_int64(value_t* v, int64_t l);
void value_set_uint64(value_t* v, uint64_t ull);
void value_set_long_double(value_t* v, long double d);
void value_set_bool(value_t* v, bool b);
void value_set_array(value_t* v, vector_t* ve);
void value_set_map(value_t* v, dict_t* d);
void value_set_constant_char(value_t* v, char* c);
// 1 success 0 fail
int value_set_binary(value_t* v, value_type_enum type,char* s, int len);

#define value_is_int64(v) (v != NULL && v->type == VALUE_INT)
#define value_is_uint64(v) (v != NULL && v->type == VALUE_UINT)
#define value_is_long_double(v) (v!= NULL && v->type == VALUE_DOUBLE)
#define value_is_bool(v) (v != NULL && v->type == VALUE_BOOLEAN)
#define value_is_sds(v)  (v != NULL && v->type == VALUE_SDS)
#define value_is_array(v) (v != NULL && v->type == VALUE_ARRAY)
#define value_is_map(v) (v != NULL &&  v->type == VALUE_MAP)
#define value_is_constant_char(v) (v != NULL && v->type == VALUE_CONSTANT_CHAR)

#define value_is_null(v) (v == NULL || v->type == VALUE_UNDEFINED)


int value_cmp(void* a, void* b);
int private_value_cmp(value_t* a, value_t* b);

#endif
