/**
 * @file value.c
 * @brief 通用值类型实现，支持多种数据类型的统一存储和操作
 */
#include "value.h"
#include "iterator/iterator.h"
#include "utils/utils.h"
#include "log/log.h"

/**
 * @brief 创建新的值对象
 * @return 返回初始化为未定义类型的值对象指针
 */
value_t* value_new() {
    value_t* v = zmalloc(sizeof(value_t));
    v->type = VALUE_UNDEFINED;
    return v;
}

/**
 * @brief 清理值对象的内容，释放内部资源
 * @param v 要清理的值对象指针
 */
void value_clean(value_t* v) {
    switch (v->type) {
        case VALUE_SDS:
            sds_delete(v->value.sds_value);
            v->value.sds_value = NULL;
            break;
        case VALUE_ARRAY: {
            // 遍历数组并删除所有子值对象
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

/**
 * @brief 删除值对象，释放所有内存
 * @param v 要删除的值对象指针
 */
void value_delete(value_t* v) {
    value_clean(v);
    zfree(v);
}

/**
 * @brief 设置值为SDS字符串类型
 * @param v 值对象指针
 * @param s SDS字符串
 */
void value_set_sds(value_t* v, sds_t s) {
    value_clean(v);
    v->type = VALUE_SDS;
    v->value.sds_value = s;
}

/**
 * @brief 设置值为64位有符号整数类型
 * @param v 值对象指针
 * @param l 64位有符号整数值
 */
void value_set_int64(value_t* v, int64_t l) {
    value_clean(v);
    v->type = VALUE_INT;
    v->value.i64_value = l;
}

/**
 * @brief 设置值为64位无符号整数类型
 * @param v 值对象指针
 * @param l 64位无符号整数值
 */
void value_set_uint64(value_t* v, uint64_t l) {
    value_clean(v);
    v->type = VALUE_UINT;
    v->value.u64_value = l;
}

/**
 * @brief 设置值为长双精度浮点数类型
 * @param v 值对象指针
 * @param d 长双精度浮点数值
 */
void value_set_long_double(value_t* v, long double d) {
    value_clean(v);
    v->type = VALUE_DOUBLE;
    v->value.ld_value = d;
}

/**
 * @brief 设置值为布尔类型
 * @param v 值对象指针
 * @param b 布尔值
 */
void value_set_bool(value_t* v, bool b) {
    value_clean(v);
    v->type = VALUE_BOOLEAN;
    v->value.bool_value = b;
}

/**
 * @brief 设置值为数组类型
 * @param v 值对象指针
 * @param ve 向量（数组）对象指针
 */
void value_set_array(value_t* v, vector_t* ve) {
    value_clean(v);
    v->type = VALUE_ARRAY;
    v->value.array_value = ve;
}

/**
 * @brief 设置值为映射（字典）类型
 * @param v 值对象指针
 * @param d 字典对象指针
 */
void value_set_map(value_t* v, dict_t* d) {
    value_clean(v);
    v->type = VALUE_MAP;
    v->value.map_value = d;
}

/**
 * @brief 设置值为常量字符串类型
 * @param v 值对象指针
 * @param c 常量字符串指针
 */
void value_set_constant_char(value_t* v, char* c) {
    value_clean(v);
    v->type = VALUE_CONSTANT_CHAR;
    v->value.sds_value = c;
}

/**
 * @brief 获取SDS字符串值
 * @param v 值对象指针
 * @return 返回SDS字符串
 */
sds_t value_get_sds(value_t* v) {
    latte_assert_with_info(value_is_sds(v), "value is not sds");
    return v->value.sds_value;
}

/**
 * @brief 获取64位有符号整数值
 * @param v 值对象指针
 * @return 返回64位有符号整数
 */
int64_t value_get_int64(value_t* v) {
    latte_assert_with_info(value_is_int64(v), "value is not int64\n");
    return v->value.i64_value;
}

/**
 * @brief 获取64位无符号整数值
 * @param v 值对象指针
 * @return 返回64位无符号整数
 */
uint64_t value_get_uint64(value_t* v) {
    latte_assert_with_info(value_is_uint64(v), "value is not uint64");
    return v->value.i64_value;
}

/**
 * @brief 获取长双精度浮点数值
 * @param v 值对象指针
 * @return 返回长双精度浮点数
 */
long double value_get_long_double(value_t* v) {
    latte_assert_with_info(value_is_long_double(v), "value is not long double");
    return v->value.ld_value;
}

/**
 * @brief 获取布尔值
 * @param v 值对象指针
 * @return 返回布尔值
 */
bool value_get_bool(value_t* v) {
    latte_assert_with_info(value_is_bool(v), "value is not boolean");
    return v->value.bool_value;
}

/**
 * @brief 获取数组值
 * @param v 值对象指针
 * @return 返回向量对象指针
 */
vector_t* value_get_array(value_t* v) {
    latte_assert_with_info(value_is_array(v), "value is not array");
    return v->value.array_value;
}

/**
 * @brief 获取映射（字典）值
 * @param v 值对象指针
 * @return 返回字典对象指针
 */
dict_t* value_get_map(value_t* v) {
    latte_assert_with_info(value_is_map(v), "value is not map");
    return v->value.map_value;
}

/**
 * @brief 获取常量字符串值
 * @param v 值对象指针
 * @return 返回常量字符串指针
 */
char* value_get_constant_char(value_t* v) {
    latte_assert_with_info(value_is_constant_char(v), "value is not constant char");
    return v->value.sds_value;
}

/**
 * @brief 获取值的二进制表示
 * @param v 值对象指针
 * @return 返回包含二进制数据的SDS字符串，失败返回NULL
 */
sds_t value_get_binary(value_t* v) {
    switch (v->type)
    {
    case VALUE_CONSTANT_CHAR:
        return sds_new(v->value.sds_value);
    case VALUE_SDS:
        return v->value.sds_value;
        break;
    case VALUE_INT:
        // 将整数值转换为二进制数据
        return sds_new_len((const char*)&v->value.i64_value, sizeof(int64_t));
    case VALUE_UINT:
        // 将无符号整数值转换为二进制数据
        return sds_new_len((const char*)&v->value.u64_value, sizeof(uint64_t));
    case VALUE_DOUBLE:
        // 将浮点数值转换为二进制数据
        return sds_new_len((const char*)&v->value.ld_value, sizeof(long double));
    case VALUE_BOOLEAN:
        // 将布尔值转换为二进制数据
        return sds_new_len((const char*)&v->value.bool_value, sizeof(int));
    default:
        LATTE_LIB_LOG(LOG_ERROR, "[value_get_binary] unsupport data type: %d", v->type);
        break;
    }

    return NULL;
}

/**
 * @brief 从二进制数据设置值
 * @param v 值对象指针
 * @param type 要设置的值类型
 * @param data 二进制数据指针
 * @param len 数据长度
 * @return 成功返回1，失败返回0
 */
int value_set_binary(value_t* v, value_type_enum type, char* data, int len) {
    switch (type)
    {
    case VALUE_SDS:
        value_set_sds(v, sds_new_len(data, len));
        break;
    case VALUE_INT:
        // 从二进制数据恢复整数值
        value_set_int64(v, *(int64_t*)data);
        break;
    case VALUE_UINT:
        // 从二进制数据恢复无符号整数值
        value_set_uint64(v, *(uint64_t*)data);
        break;
    case VALUE_DOUBLE:
        // 从二进制数据恢复浮点数值
        value_set_long_double(v, *(long double*)data);
        break;
    case VALUE_BOOLEAN:
        // 从二进制数据恢复布尔值
        value_set_bool(v, *(int *)data != 0);
        break;
    default:
        LATTE_LIB_LOG(LOG_ERROR, "[value_set_binary] unsupport or unknown data type: %d", type);
        return 0;
        break;
    }
    return 1;
}

/**
 * @brief 值比较函数（通用接口）
 * @param a 第一个值对象指针
 * @param b 第二个值对象指针
 * @return 比较结果：<0表示a<b，=0表示a=b，>0表示a>b
 */
int value_cmp(void* a, void* b) {
    return private_value_cmp((value_t*)a, (value_t*)b);
}

/**
 * @brief 内部值比较函数实现
 * @param a 第一个值对象指针
 * @param b 第二个值对象指针
 * @return 比较结果：<0表示a<b，=0表示a=b，>0表示a>b
 */
int private_value_cmp(value_t* a, value_t* b) {
    // 首先比较类型
    if (a->type != b->type) {
        return a->type - b->type;
    }

    // 根据具体类型进行值比较
    switch (a->type)
    {
    case VALUE_UNDEFINED:
        return 0; // 未定义类型都相等
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