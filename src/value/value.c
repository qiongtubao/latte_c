#include "value.h"
#include "iterator/iterator.h"
#include "utils/utils.h"
#include "log/log.h"
value_t* value_new() {
    value_t* v = zmalloc(sizeof(value_t));
    v->type = VALUE_UNDEFINED;
    return v;
}

void valueClean(value_t* v) {
    switch (v->type) {
        case VALUE_SDS:
            sds_free(v->value.sds_value);
            v->value.sds_value = NULL;
            break;
        case VALUE_ARRAY: {
            Iterator* it = vector_get_iterator(v->value.array_value);
            while(iteratorHasNext(it)) {
                value_t* cv = iteratorNext(it);
                value_delete(cv);
            }
            iteratorRelease(it);
            vector_delete(v->value.array_value);
            v->value.array_value = NULL;
        }
            break;
        case VALUE_MAP:
            dict_delete(v->value.map_value);
            v->value.map_value = NULL;
        default:
        break;
    }
    v->type = VALUE_UNDEFINED;
}
void value_delete(value_t* v) {
    valueClean(v);
    zfree(v);
}

void value_set_sds(value_t* v, sds_t s) {
    valueClean(v);
    v->type = VALUE_SDS;
    v->value.sds_value = s;
}

void value_set_int64(value_t* v, int64_t l) {
    valueClean(v);
    v->type = VALUE_INT;
    v->value.i64_value = l;
}

void value_set_uint64(value_t* v, uint64_t l) {
    valueClean(v);
    v->type = VALUE_UINT;
    v->value.u64_value = l;
}

void value_set_longdouble(value_t* v, long double d) {
    valueClean(v);
    v->type = VALUE_DOUBLE;
    v->value.ld_value = d;
}
void value_set_bool(value_t* v, bool b) {
    valueClean(v);
    v->type = VALUE_BOOLEAN;
    v->value.bool_value = b;
}
void value_set_array(value_t* v, vector_t* ve) {
    valueClean(v);
    v->type = VALUE_ARRAY;
    v->value.array_value = ve;
}
void value_set_map(value_t* v, dict_t* d) {
    valueClean(v);
    v->type = VALUE_MAP;
    v->value.map_value = d;
}


sds_t value_get_sds(value_t* v) {
    latte_assert(value_is_sds(v), "value is not sds");
    return v->value.sds_value;
}

int64_t value_get_int64(value_t* v) {
    latte_assert(value_is_int64(v), "value is not int64\n");
    return v->value.i64_value;
}

uint64_t value_get_uint64(value_t* v) {
    latte_assert(value_is_uint64(v), "value is not uint64");
    return v->value.i64_value;
}

long double value_get_longdouble(value_t* v) {
    latte_assert(value_is_longdouble(v), "value is not long double");
    return v->value.ld_value;
}
bool value_get_bool(value_t* v) {
    latte_assert(value_is_bool(v), "value is not boolean");
    return v->value.bool_value;
}
vector_t* value_get_array(value_t* v) {
    latte_assert(value_is_array(v), "value is not array");
    return v->value.array_value;
}
dict_t* value_get_map(value_t* v) {
    latte_assert(value_is_map(v), "value is not map");
    return v->value.map_value;
}

sds_t value_get_binary(value_t* v) {
    switch (v->type)
    {
    case VALUE_SDS:
        return v->value.sds_value;
        /* code */
        break;
    case VALUE_INT:
        return sds_new_len((const char*)&v->value.i64_value, sizeof(int64_t));
    case VALUE_UINT:
        return sds_new_len((const char*)&v->value.u64_value, sizeof(uint64_t));
    case VALUE_DOUBLE:
        return sds_new_len((const char*)&v->value.ld_value, sizeof(long double));
    case VALUE_BOOLEAN:
        return sds_new_len((const char*)&v->value.bool_value, sizeof(int));
    default:
        LATTE_LIB_LOG(LOG_ERROR, "[value_get_binary] unsupport data type: %d", v->type);
        break;
    }

    return NULL;
}

/**
 *   1 success
 *   0 fail
 */
int value_set_binary(value_t* v, value_type_enum type, char* data, int len) {
    switch (type)
    {
    case VALUE_SDS:
        value_set_sds(v, sds_new_len(data, len));
        break;
    case VALUE_INT:
        value_set_int64(v, *(int64_t*)data);
        break;
    case VALUE_UINT:
        value_set_uint64(v, *(uint64_t*)data);
        break;
    case VALUE_DOUBLE:
        value_set_longdouble(v, *(long double*)data);
        break;
    case VALUE_BOOLEAN:
        value_set_bool(v, *(int *)data != 0);
        break;
    default:
        LATTE_LIB_LOG(LOG_ERROR, "[value_set_binary] unsupport or unknown data type: %d", type);
        return 0;
        break;
    }
    return 1;
}
