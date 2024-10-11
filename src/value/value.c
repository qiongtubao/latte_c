#include "value.h"
#include "iterator/iterator.h"
#include "utils/utils.h"
#include "log/log.h"
value* valueCreate() {
    value* v = zmalloc(sizeof(value));
    v->type = VALUE_UNDEFINED;
    return v;
}

void valueClean(value* v) {
    switch (v->type) {
        case VALUE_SDS:
            sdsfree(v->value.sds_value);
            v->value.sds_value = NULL;
            break;
        case VALUE_ARRAY: {
            Iterator* it = vectorGetIterator(v->value.array_value);
            while(iteratorHasNext(it)) {
                value* cv = iteratorNext(it);
                valueRelease(cv);
            }
            iteratorRelease(it);
            vectorRelease(v->value.array_value);
            v->value.array_value = NULL;
        }
            break;
        case VALUE_MAP:
            dictRelease(v->value.map_value);
            v->value.map_value = NULL;
        default:
        break;
    }
    v->type = VALUE_UNDEFINED;
}
void valueRelease(value* v) {
    valueClean(v);
    zfree(v);
}

void valueSetSds(value* v, sds s) {
    valueClean(v);
    v->type = VALUE_SDS;
    v->value.sds_value = s;
}

void valueSetInt64(value* v, int64_t l) {
    valueClean(v);
    v->type = VALUE_INT;
    v->value.i64_value = l;
}

void valueSetUInt64(value* v, uint64_t l) {
    valueClean(v);
    v->type = VALUE_UINT;
    v->value.u64_value = l;
}

void valueSetLongDouble(value* v, long double d) {
    valueClean(v);
    v->type = VALUE_DOUBLE;
    v->value.ld_value = d;
}
void valueSetBool(value* v, bool b) {
    valueClean(v);
    v->type = VALUE_BOOLEAN;
    v->value.bool_value = b;
}
void valueSetArray(value* v, vector* ve) {
    valueClean(v);
    v->type = VALUE_ARRAY;
    v->value.array_value = ve;
}
void valueSetMap(value* v, dict* d) {
    valueClean(v);
    v->type = VALUE_MAP;
    v->value.map_value = d;
}


sds valueGetSds(value* v) {
    latte_assert(valueIsSds(v), "value is not sds");
    return v->value.sds_value;
}

int64_t valueGetInt64(value* v) {
    latte_assert(valueIsInt64(v), "value is not int64\n");
    return v->value.i64_value;
}

uint64_t valueGetUInt64(value* v) {
    latte_assert(valueIsUInt64(v), "value is not uint64");
    return v->value.i64_value;
}

long double valueGetLongDouble(value* v) {
    latte_assert(valueIsLongDouble(v), "value is not long double");
    return v->value.ld_value;
}
bool valueGetBool(value* v) {
    latte_assert(valueIsBool(v), "value is not boolean");
    return v->value.bool_value;
}
vector* valueGetArray(value* v) {
    latte_assert(valueIsArray(v), "value is not array");
    return v->value.array_value;
}
dict* valueGetMap(value* v) {
    latte_assert(valueIsMap(v), "value is not map");
    return v->value.map_value;
}

sds valueGetBinary(value* v) {
    switch (v->type)
    {
    case VALUE_SDS:
        return v->value.sds_value;
        /* code */
        break;
    case VALUE_INT:
        return sdsnewlen((const char*)&v->value.i64_value, sizeof(int64_t));
    case VALUE_UINT:
        return sdsnewlen((const char*)&v->value.u64_value, sizeof(uint64_t));
    case VALUE_DOUBLE:
        return sdsnewlen((const char*)&v->value.ld_value, sizeof(long double));
    case VALUE_BOOLEAN:
        return sdsnewlen((const char*)&v->value.bool_value, sizeof(int));
    default:
        LATTE_LIB_LOG(LOG_ERROR, "[valueGetBinary] unsupport data type: %d", v->type);
        break;
    }

    return NULL;
}

/**
 *   1 success
 *   0 fail
 */
int valueSetBinary(value* v, valueType type, char* data, int len) {
    switch (type)
    {
    case VALUE_SDS:
        valueSetSds(v, sdsnewlen(data, len));
        break;
    case VALUE_INT:
        valueSetInt64(v, *(int64_t*)data);
        break;
    case VALUE_UINT:
        valueSetUInt64(v, *(uint64_t*)data);
        break;
    case VALUE_DOUBLE:
        valueSetLongDouble(v, *(long double*)data);
        break;
    case VALUE_BOOLEAN:
        valueSetBool(v, *(int *)data != 0);
        break;
    default:
        LATTE_LIB_LOG(LOG_ERROR, "[valueSetBinary] unsupport or unknown data type: %d", type);
        return 0;
        break;
    }
    return 1;
}
