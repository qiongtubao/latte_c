/**
 * @file latte_json.h
 * @brief JSON 序列化/反序列化模块接口
 *        提供 JSON map（对象）和 JSON array（数组）的构建、读取、序列化及解析功能。
 *        底层基于 value_t 类型，通过 sds 字符串传递键名和字符串值。
 */
#ifndef __LATTE_JSON_H
#define __LATTE_JSON_H

#include "sds/sds.h"
#include "vector/vector.h"
#include "value/value.h"

/** @brief JSON 节点类型别名（底层为 value_t） */
#define json_t value_t

/**
 * @brief 严格模式标志：启用后对 JSON 输入进行严格校验
 *        注意：启用此标志可能因标准收紧导致之前合法的输入失败；
 *        同时会禁用在单个字符流中解析多个 JSON 对象的能力
 *        （若需支持，可同时设置 JSON_TOKENER_ALLOW_TRAILING_CHARS）。
 *        默认不启用。
 */
#define JSON_TOKENER_STRICT 0x01

/**
 * @brief 允许尾随字符标志：与 JSON_TOKENER_STRICT 联用，
 *        允许在第一个合法 JSON 对象之后存在额外字符。
 *        默认不启用。
 */
#define JSON_TOKENER_ALLOW_TRAILING_CHARS 0x02

/* ---- JSON Map（对象）API ---- */

/**
 * @brief 创建空的 JSON map 节点
 * @return 新建的 json_t 指针（map 类型）
 */
json_t* json_map_new();

/**
 * @brief 向 JSON map 中插入任意 json_t 值
 * @param root 目标 map 节点
 * @param key  键名（sds 字符串）
 * @param v    值节点（任意 json_t 类型）
 * @return 成功返回 0；失败返回 -1
 */
int json_map_put_value(json_t* root, sds_t key, json_t* v);

/**
 * @brief 向 JSON map 中插入字符串值
 * @param root 目标 map 节点
 * @param key  键名（sds 字符串）
 * @param sv   字符串值（sds）
 * @return 成功返回 0；失败返回 -1
 */
int json_map_put_sds(json_t* root, sds_t key, sds_t sv);

/**
 * @brief 向 JSON map 中插入 int64 整数值
 * @param root 目标 map 节点
 * @param key  键名（sds 字符串）
 * @param sv   int64_t 整数值
 * @return 成功返回 0；失败返回 -1
 */
int json_map_put_int64(json_t* root, sds_t key, int64_t sv);

/**
 * @brief 向 JSON map 中插入布尔值
 * @param root 目标 map 节点
 * @param key  键名（sds 字符串）
 * @param v    布尔值
 * @return 成功返回 0；失败返回 -1
 */
int json_map_put_bool(json_t* root, sds_t key, bool v);

/**
 * @brief 向 JSON map 中插入 long double 浮点值
 * @param root 目标 map 节点
 * @param key  键名（sds 字符串）
 * @param v    long double 值
 * @return 成功返回 0；失败返回 -1
 */
int json_map_put_longdouble(json_t* root, sds_t key, long double v);

/**
 * @brief 从 JSON map 中按键名获取值节点
 * @param root 目标 map 节点
 * @param key  键名（C 字符串）
 * @return 对应的 json_t 值节点；键不存在返回 NULL
 */
json_t* json_map_get_value(json_t* root, char* key);

/** @brief 从 JSON map 获取字符串值（sds） */
#define json_map_get_sds(root, key)        value_get_sds(json_map_get_value(root, key))
/** @brief 从 JSON map 获取 int64 整数值 */
#define json_map_get_int64(root, key)      value_get_int64(json_map_get_value(root, key))
/** @brief 从 JSON map 获取布尔值 */
#define json_map_get_bool(root, key)       value_get_bool(json_map_get_value(root, key))
/** @brief 从 JSON map 获取 long double 浮点值 */
#define json_map_get_longdouble(root, key) value_get_longdouble(json_map_get_value(root, key))

/* ---- JSON Array（数组）API ---- */

/**
 * @brief 创建空的 JSON array 节点
 * @return 新建的 json_t 指针（array 类型）
 */
json_t* json_array_new();

/**
 * @brief 向 JSON array 末尾追加字符串元素
 * @param v       目标 array 节点
 * @param element 字符串元素（sds）
 * @return 成功返回 0；失败返回 -1
 */
int json_array_put_sds(json_t* v, sds_t element);

/**
 * @brief 向 JSON array 末尾追加布尔元素
 * @param root    目标 array 节点
 * @param element 布尔值
 * @return 成功返回 0；失败返回 -1
 */
int json_array_put_bool(json_t* root, bool element);

/**
 * @brief 向 JSON array 末尾追加 int64 整数元素
 * @param root    目标 array 节点
 * @param element int64_t 整数值
 * @return 成功返回 0；失败返回 -1
 */
int json_array_put_int64(json_t* root, int64_t element);

/**
 * @brief 向 JSON array 末尾追加 long double 浮点元素
 * @param root    目标 array 节点
 * @param element long double 值
 * @return 成功返回 0；失败返回 -1
 */
int json_array_put_longdouble(json_t* root, long double element);

/**
 * @brief 向 JSON array 末尾追加任意 json_t 节点
 * @param root    目标 array 节点
 * @param element 要追加的 json_t 节点
 * @return 成功返回 0；失败返回 -1
 */
int json_array_put_value(json_t* root, json_t* element);

/**
 * @brief 将 JSON array 收缩到指定最大长度（截断尾部多余元素）
 * @param root 目标 array 节点
 * @param max  保留的最大元素数量
 * @return 成功返回 0；失败返回 -1
 */
int json_array_shrink(json_t* root, int max);

/**
 * @brief 将 JSON array 调整为指定大小（扩容或截断）
 * @param root 目标 array 节点
 * @param size 目标元素数量
 */
void json_array_resize(json_t* root, int size);

/* ---- 序列化 / 反序列化 ---- */

/**
 * @brief 将 JSON 节点序列化为 sds 字符串
 * @param v 要序列化的 json_t 节点
 * @return 序列化后的 sds 字符串；调用方负责用 sds_free 释放
 */
sds_t json_to_sds(json_t* v);

/**
 * @brief JSON 打印缓冲区（内部序列化使用）
 */
typedef struct json_printbuf_t {
    char *buf;   /**< 缓冲区指针 */
    int bpos;    /**< 当前写入位置 */
    int size;    /**< 缓冲区总容量 */
} json_printbuf_t;

/**
 * @brief 创建 JSON 打印缓冲区
 * @return 新建的 json_printbuf_t 指针
 */
json_printbuf_t* json_printbuf_new();

/**
 * @brief 释放 JSON 打印缓冲区
 * @param buf 要释放的缓冲区指针
 */
void json_printbuf_delete(json_printbuf_t* buf);

/**
 * @brief UTF-8 验证标志：启用后解析时校验输入是否为合法 UTF-8；
 *        若验证失败，json_tokener_get_error() 返回
 *        json_tokener_error_parse_utf8_string。
 *        默认不启用。
 */
#define JSON_TOKENER_VALIDATE_UTF8 0x10

/**
 * @brief JSON 解析错误枚举
 */
typedef enum json_tokener_error_enum {
    json_tokener_success,                       /**< 解析成功 */
    json_tokener_continue,                      /**< 需要更多输入（流式解析） */
    json_tokener_error_depth,                   /**< 嵌套深度超限 */
    json_tokener_error_parse_eof,               /**< 意外的输入结束 */
    json_tokener_error_parse_unexpected,        /**< 遇到意外字符 */
    json_tokener_error_parse_null,              /**< null 值解析失败 */
    json_tokener_error_parse_boolean,           /**< boolean 值解析失败 */
    json_tokener_error_parse_number,            /**< 数字解析失败 */
    json_tokener_error_parse_array,             /**< 数组解析失败 */
    json_tokener_error_parse_object_key_name,   /**< 对象键名解析失败 */
    json_tokener_error_parse_object_key_sep,    /**< 对象键值分隔符解析失败 */
    json_tokener_error_parse_object_value_sep,  /**< 对象值分隔符解析失败 */
    json_tokener_error_parse_string,            /**< 字符串解析失败 */
    json_tokener_error_parse_comment,           /**< 注释解析失败 */
    json_tokener_error_parse_utf8_string,       /**< UTF-8 验证失败 */
    json_tokener_error_size,                    /**< 输入字符串长度超过 INT32_MAX */
    json_tokener_error_memory                   /**< 内存分配失败 */
} json_tokener_error_enum;

/**
 * @brief JSON 解析器状态枚举
 * @deprecated 不应在 json_tokener.c 外部使用，后续版本将设为私有
 */
typedef enum json_tokener_state_enum {
    json_tokener_state_eatws,                           /**< 跳过空白字符 */
    json_tokener_state_start,                           /**< 开始解析 */
    json_tokener_state_finish,                          /**< 解析完成 */
    json_tokener_state_null,                            /**< 解析 null */
    json_tokener_state_comment_start,                   /**< 注释开始 */
    json_tokener_state_comment,                         /**< 注释内容 */
    json_tokener_state_comment_eol,                     /**< 行注释结束 */
    json_tokener_state_comment_end,                     /**< 块注释结束 */
    json_tokener_state_string,                          /**< 字符串内容 */
    json_tokener_state_string_escape,                   /**< 字符串转义 */
    json_tokener_state_enumscape_unicode,               /**< Unicode 转义 */
    json_tokener_state_enumscape_unicode_need_escape,   /**< Unicode 代理对待转义 */
    json_tokener_state_enumscape_unicode_need_u,        /**< Unicode 代理对待 u */
    json_tokener_state_boolean,                         /**< 布尔值 */
    json_tokener_state_number,                          /**< 数字 */
    json_tokener_state_array,                           /**< 数组开始 */
    json_tokener_state_array_add,                       /**< 数组添加元素 */
    json_tokener_state_array_sep,                       /**< 数组分隔符 */
    json_tokener_state_object_field_start,              /**< 对象字段开始 */
    json_tokener_state_object_field,                    /**< 对象字段名 */
    json_tokener_state_object_field_end,                /**< 对象字段名结束 */
    json_tokener_state_object_value,                    /**< 对象字段值 */
    json_tokener_state_object_value_add,                /**< 对象字段值添加 */
    json_tokener_state_object_sep,                      /**< 对象字段分隔符 */
    json_tokener_state_array_after_sep,                 /**< 数组分隔符后 */
    json_tokener_state_object_field_start_after_sep,    /**< 对象分隔符后字段开始 */
    json_tokener_state_inf                              /**< 无穷大值 */
} json_tokener_state_enum;

/**
 * @brief JSON 解析器状态帧（解析栈元素）
 * @deprecated 不应在 json_tokener.c 外部使用，后续版本将设为私有
 */
typedef struct json_tokener_srec_t {
    enum json_tokener_state_enum state;        /**< 当前解析状态 */
    enum json_tokener_state_enum saved_state;  /**< 上一个保存的状态（用于状态恢复） */
    value_t *obj;                              /**< 当前正在构建的顶层对象 */
    value_t *current;                          /**< 当前正在构建的子对象 */
    sds_t obj_field_name;                      /**< 当前对象字段名 */
} json_tokener_srec_t;

/**
 * @brief 创建 JSON 解析器状态帧
 * @return 新建的 json_tokener_srec_t 指针
 */
json_tokener_srec_t* jsonTokenerSrecCreate();

/**
 * @brief JSON 解析器（Tokener）主结构体
 * @deprecated 不应直接访问内部字段，后续版本将封装为私有
 */
typedef struct json_tokener_t {
    char *str;                  /**< 当前解析位置指针 */
    struct json_printbuf_t *pb; /**< 临时打印缓冲区（字符串累积） */
    int max_depth;              /**< 最大嵌套深度 */
    int depth;                  /**< 当前嵌套深度 */
    int is_double;              /**< 当前数字是否为浮点 */
    int st_pos;                 /**< 状态帧索引 */
    int char_offset;            /**< 当前字符偏移量（已废弃，请用 json_tokener_get_parse_end()） */
    json_tokener_error_enum err; /**< 当前错误码（已废弃，请用 json_tokener_get_error()） */
    unsigned int ucs_char;      /**< 当前 Unicode 码点 */
    unsigned int high_surrogate;/**< 高代理项（用于 UTF-16 代理对） */
    char quote_char;            /**< 当前字符串引号字符 */
    vector_t* stack;            /**< 解析状态栈（json_tokener_srec_t 数组） */
    int flags;                  /**< 解析标志位（JSON_TOKENER_STRICT 等） */
} json_tokener_t;

/**
 * @brief 创建 JSON 解析器实例
 * @return 新建的 json_tokener_t 指针
 */
json_tokener_t* json_tokener_new();

/**
 * @brief 释放 JSON 解析器实例
 * @param tokener 要释放的解析器指针
 */
void json_tokener_delete(json_tokener_t* tokener);

/**
 * @brief 将 sds 字符串解析为 JSON 节点
 * @param str    输入 JSON 字符串（sds）
 * @param result 输出解析结果的 json_t 指针地址（成功时填充）
 * @return 成功返回 0（json_tokener_success）；失败返回对应错误码
 */
int sds_to_json(sds_t str, json_t** result);

#endif
