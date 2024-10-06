#include "value.h"
#include "iterator/iterator.h"
#include "utils/utils.h"

value* valueCreate() {
    value* v = zmalloc(sizeof(value));
    v->type = UNDEFINED;
    v->value.ll_value = 0;
    return v;
}

void valueClean(value* v) {
    switch (v->type) {
        case SDSS:
            sdsfree(v->value.sds_value);
            v->value.sds_value = NULL;
            break;
        case LISTTYPS: {
            Iterator* it = vectorGetIterator(v->value.list_value);
            while(iteratorHasNext(it)) {
                value* cv = iteratorNext(it);
                valueRelease(cv);
            }
            iteratorRelease(it);
            vectorRelease(v->value.list_value);
            v->value.list_value = NULL;
        }
            break;
        case MAPTYPES:
            dictRelease(v->value.map_value);
            v->value.map_value = NULL;
        default:
        
    }
}
void valueRelease(value* v) {
    valueClean(v);
    zfree(v);
}

void valueSetSds(value* v, sds s) {
    valueClean(v);
    v->type = SDSS;
    v->value.sds_value = s;
}

void valueSetInt64(value* v, int64_t l) {
    valueClean(v);
    v->type = INTS;
    v->value.ll_value = l;
}

void valueSetUint64(value* v, uint64_t l) {
    valueClean(v);
    v->type = UINTS;
    v->value.ll_value = l;
}

void valueSetLongDouble(value* v, long double d) {
    valueClean(v);
    v->type = DOUBLES;
    v->value.ld_value = d;
}
void valueSetBool(value* v, bool b) {
    valueClean(v);
    v->type = BOOLEANS;
    v->value.bool_value = b;
}
void valueSetArray(value* v, vector* ve) {
    valueClean(v);
    v->type = LISTTYPS;
    v->value.list_value = ve;
}
void valueSetDict(value* v, dict* d) {
    valueClean(v);
    v->type = MAPTYPES;
    v->value.map_value = d;
}


sds valueGetSds(value* v) {
    latte_assert(v->type == SDSS, "value is not sds");
    return v->value.sds_value;
}

int64_t valueGetInt64(value* v) {
    latte_assert(v->type == INTS, "value is not int64");
    return v->value.ll_value;
}

uint64_t valueGetUint64(value* v) {
    latte_assert(v->type == UINTS, "value is not uint64");
    return v->value.ull_value;
}

long double valueGetLongDouble(value* v) {
    latte_assert(v->type == DOUBLES, "value is not long double");
    return v->value.ld_value;
}
bool valueGetBool(value* v) {
    latte_assert(v->type == BOOLEANS, "value is not boolean");
    return v->value.bool_value;
}
vector* valueGetArray(value* v) {
    latte_assert(v->type == LISTTYPS, "value is not int64");
    return v->value.list_value;
}
dict* valueGetDict(value* v) {
    latte_assert(v->type == MAPTYPES, "value is not int64");
    return v->value.map_value;
}


