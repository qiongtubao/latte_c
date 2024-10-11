#ifndef __LATTE_VALUE_H
#define __LATTE_VALUE_H

#include "sds/sds.h"
#include "vector/vector.h"
#include <stdbool.h>
#include "dict/dict.h"


typedef enum valueType {
    UNDEFINED,
    SDSS,
    INTS,
    UINTS,
    DOUBLES,
    BOOLEANS,
    LISTTYPS,
    MAPTYPES,
} valueType;
typedef struct value {
    valueType type;
    union {
        sds sds_value;
        int64_t ll_value;
        uint64_t  ull_value;
        long double ld_value;
        bool bool_value;
        vector* list_value;
        dict* map_value;
    } value;
} value;

value* valueCreate();
void valueRelease(value* v);


sds valueGetSds(value* v);
int64_t valueGetInt64(value* v);
uint64_t valueGetUint64(value* v);
long double valueGetLongDouble(value* v);
bool valueGetBool(value* v);
vector* valueGetArray(value* v);
dict* valueGetDict(value* v);

void valueSetSds(value* v, sds s);
void valueSetInt64(value* v, int64_t l);
void valueSetUint64(value* v, uint64_t ull);
void valueSetLongDouble(value* v, long double d);
void valueSetBool(value* v, bool b);
void valueSetArray(value* v, vector* ve);
void valueSetDict(value* v, dict* d);

#define valueIsInt64(v) (v != NULL && v->type == INTS)
#define valueIsUInt64(v) (v != NULL && v->type == UINTS)
#define valueIsLongDouble(v) (v!= NULL && v->type == DOUBLES)
#define valueIsBool(v) (v != NULL && v->type == BOOLEANS)
#define valueIsSds(v)  (v != NULL && v->type == SDSS)
#define valueIsArray(v) (v != NULL && v->type == LISTTYPS)
#define valueIsMap(v) (v != NULL &&  v->type == MAPTYPES)

#endif