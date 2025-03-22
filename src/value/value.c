#include "value.h"
#include "iterator/iterator.h"
#include "utils/utils.h"
#include "log/log.h"
value_t* value_new() {
    value_t* v = zmalloc(sizeof(value_t));
    v->type = VALUE_UNDEFINED;
    return v;
}

void value_clean(value_t* v) {
    switch (v->type) {
        case VALUE_SDS:
            sds_delete(v->value.sds_value);
            v->value.sds_value = NULL;
            break;
        case VALUE_ARRAY: {
            latte_iterator_t* it = vector_get_iterator(v->value.array_value);
            while(latte_iterator_has_next(it)) {
                value_t* cv = latte_iterator_next(it);
                value_delete(cv);
            }
            latte_iterator_delete(it);
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
    value_clean(v);
    zfree(v);
}

void value_set_sds(value_t* v, sds_t s) {
    value_clean(v);
    v->type = VALUE_SDS;
    v->value.sds_value = s;
}

void value_set_int64(value_t* v, int64_t l) {
    value_clean(v);
    v->type = VALUE_INT;
    v->value.i64_value = l;
}

void value_set_uint64(value_t* v, uint64_t l) {
    value_clean(v);
    v->type = VALUE_UINT;
    v->value.u64_value = l;
}

void value_set_long_double(value_t* v, long double d) {
    value_clean(v);
    v->type = VALUE_DOUBLE;
    v->value.ld_value = d;
}
void value_set_bool(value_t* v, bool b) {
    value_clean(v);
    v->type = VALUE_BOOLEAN;
    v->value.bool_value = b;
}
void value_set_array(value_t* v, vector_t* ve) {
    value_clean(v);
    v->type = VALUE_ARRAY;
    v->value.array_value = ve;
}
void value_set_map(value_t* v, dict_t* d) {
    value_clean(v);
    v->type = VALUE_MAP;
    v->value.map_value = d;
}

void value_set_constant_char(value_t* v, char* c) {
    value_clean(v);
    v->type = VALUE_CONSTANT_CHAR;
    v->value.sds_value = c;
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

long double value_get_long_double(value_t* v) {
    latte_assert(value_is_long_double(v), "value is not long double");
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

char* value_get_constant_char(value_t* v) {
    latte_assert(value_is_constant_char(v), "value is not constant char");
    return v->value.sds_value;
}

sds_t value_get_binary(value_t* v) {
    switch (v->type)
    {
    case VALUE_CONSTANT_CHAR:
        return sds_new(v->value.sds_value); 
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
        value_set_long_double(v, *(long double*)data);
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


//cmp 
int value_cmp(void* a, void* b) {
    return private_value_cmp((value_t*)a, (value_t*)b);
}
int private_value_cmp(value_t* a, value_t* b) {
    if (a->type != b->type) {
        return a->type - b->type;
    }
    switch (a->type)
    {
    case VALUE_UNDEFINED:
        return 0;
        break;
    case VALUE_CONSTANT_CHAR:
        return private_str_cmp(value_get_constant_char(a), value_get_constant_char(b));
    case VALUE_INT:
        return private_int64_cmp(value_get_int64(a), value_get_int64(b));
    case VALUE_UINT:
        return private_uint64_cmp(value_get_uint64(a), value_get_uint64(b));
    case VALUE_BOOLEAN:
        return private_int64_cmp((int64_t)value_get_bool(a) , (int64_t)value_get_bool(b));
    case VALUE_SDS:
        return sds_cmp(value_get_sds(a), value_get_sds(b));
    case VALUE_DOUBLE:
        return private_long_double_cmp(value_get_long_double(a), value_get_long_double(b));
    case VALUE_ARRAY:
        return private_vector_cmp(value_get_array(a), value_get_array(b), value_cmp);
    case VALUE_MAP:
        return private_dict_cmp(value_get_map(a), value_get_map(b), value_cmp);
    default:
        LATTE_LIB_LOG(LOG_ERROR, "[value_cmp] unsupport or unknown data type: %d", a->type);
        break;
    }
}