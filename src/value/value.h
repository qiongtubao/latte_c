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
    DOUBLES,
    BOOLEANS,
    LISTTYPS,
    MAPTYPES,
} valueType;
typedef struct value {
    valueType type;
    union {
        sds sds_value;
        long l_value;
        double d_value;
        bool bool_value;
        vector* list_value;
        dict* map_value;
    } value;
} value;

value* valueCreate();
void valueRelease(value* v);


sds valueGetSds(value* v);
long valueGetLong(value* v);
double valueGetDouble(value* v);
bool valueGetBool(value* v);
vector* valueGetArray(value* v);
dict* valueGetDict(value* v);

void valueSetSds(value* v, sds s);
void valueSetLong(value* v, long l);
void valueSetDouble(value* v, double d);
void valueSetBool(value* v, bool b);
void valueSetArray(value* v, vector* ve);
void valueSetDict(value* v, dict* d);

#endif