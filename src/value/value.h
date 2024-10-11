#ifndef __LATTE_VALUE_H
#define __LATTE_VALUE_H

#include "sds/sds.h"
#include "vector/vector.h"
#include <stdbool.h>
#include "dict/dict.h"


typedef enum valueType {
    VALUE_UNDEFINED,
    VALUE_SDS,
    VALUE_INT,
    VALUE_UINT,
    VALUE_DOUBLE,
    VALUE_BOOLEAN,
    VALUE_ARRAY,
    VALUE_MAP,
} valueType;
typedef struct value {
    valueType type;
    union {
        sds sds_value;
        int64_t i64_value;
        uint64_t  u64_value;
        long double ld_value;
        bool bool_value;
        vector* array_value;
        dict* map_value;
    } value;
} value;

value* valueCreate();
void valueRelease(value* v);


sds valueGetSds(value* v);
int64_t valueGetInt64(value* v);
uint64_t valueGetUInt64(value* v);
long double valueGetLongDouble(value* v);
bool valueGetBool(value* v);
vector* valueGetArray(value* v);
dict* valueGetMap(value* v);

void valueSetSds(value* v, sds s);
void valueSetInt64(value* v, int64_t l);
void valueSetUInt64(value* v, uint64_t ull);
void valueSetLongDouble(value* v, long double d);
void valueSetBool(value* v, bool b);
void valueSetArray(value* v, vector* ve);
void valueSetMap(value* v, dict* d);

// 1 success 0 fail
int valueSetBinary(value* v, valueType type,char* s, int len);
sds valueGetBinary(value* v);
#define valueIsInt64(v) (v != NULL && v->type == VALUE_INT)
#define valueIsUInt64(v) (v != NULL && v->type == VALUE_UINT)
#define valueIsLongDouble(v) (v!= NULL && v->type == VALUE_DOUBLE)
#define valueIsBool(v) (v != NULL && v->type == VALUE_BOOLEAN)
#define valueIsSds(v)  (v != NULL && v->type == VALUE_SDS)
#define valueIsArray(v) (v != NULL && v->type == VALUE_ARRAY)
#define valueIsMap(v) (v != NULL &&  v->type == VALUE_MAP)

#define valueIsNull(v) (v == NULL || v->type == VALUE_UNDEFINED)

#endif