#ifndef __LATTE_SDS_PLUGINS_H
#define __LATTE_SDS_PLUGINS_H

#include "sds.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 将32位无符号整数以定长方式追加到sds字符串末尾
 * @param result 目标sds字符串
 * @param value 要追加的32位无符号整数
 * @return sds_t 追加后的新sds字符串
 */
sds_t sds_append_fixed32(sds_t result, uint32_t value);

/**
 * @brief 将64位无符号整数以定长方式追加到sds字符串末尾
 * @param result 目标sds字符串
 * @param value 要追加的64位无符号整数
 * @return sds_t 追加后的新sds字符串
 */
sds_t sds_append_fixed64(sds_t result, uint64_t value);

//定量32位和64位
/**
 * @brief 将32位无符号整数编码为定长字节序列（小端序）
 * @param dst 目标缓冲区，至少需要4字节
 * @param value 要编码的32位无符号整数
 */
void encode_fixed32(char* dst, uint32_t value);

/**
 * @brief 从字节序列解码出32位无符号整数（小端序）
 * @param ptr 指向字节序列的指针
 * @return uint32_t 解码出的32位无符号整数
 */
uint32_t decode_fixed32(const char* ptr);

/**
 * @brief 将64位无符号整数编码为定长字节序列（小端序）
 * @param dst 目标缓冲区，至少需要8字节
 * @param value 要编码的64位无符号整数
 */
void encode_fixed64(char* dst, uint64_t value);

/**
 * @brief 从字节序列解码出64位无符号整数（小端序）
 * @param ptr 指向字节序列的指针
 * @return uint64_t 解码出的64位无符号整数
 */
uint64_t decode_fixed64(char* ptr);

//变量32位和64位
//返回长度   leveldb中返回最后位置
/**
 * @brief 将32位无符号整数编码为变长字节序列（Varint）
 * @param dst 目标缓冲区
 * @param v 要编码的32位无符号整数
 * @return int 编码后占用的字节数
 */
int encode_var_int32(char* dst, uint32_t v);

/**
 * @brief 字符串切片结构体，用于零拷贝读取
 */
typedef struct slice_t {
    char* p;
    int len;
} slice_t;

/**
 * @brief 从slice中解析出变长32位整数，并移动slice指针
 * @param slice 输入数据的切片，解析后内部指针会前移
 * @param value 输出参数，用于存放解析出的32位整数
 * @return bool 解析是否成功
 */
bool get_var_int32(slice_t* slice, uint32_t* value);

/**
 * @brief 将32位整数编码为Varint并追加到sds字符串末尾
 * @param result 目标sds字符串
 * @param v 要追加的32位整数
 * @return sds_t 追加后的新sds字符串
 */
sds_t sds_append_var_int32(sds_t result, uint32_t v);

/**
 * @brief 计算64位整数编码为Varint后所需的字节长度
 * @param v 64位整数
 * @return int 所需的字节数
 */
int var_int_length(uint64_t v);

/**
 * @brief 将64位无符号整数编码为变长字节序列（Varint）
 * @param dst 目标缓冲区
 * @param v 要编码的64位无符号整数
 * @return int 编码后占用的字节数
 */
int encode_var_int64(char* dst, uint64_t v);

/**
 * @brief 从slice中解析出变长64位整数，并移动slice指针
 * @param slice 输入数据的切片，解析后内部指针会前移
 * @param value 输出参数，用于存放解析出的64位整数
 * @return bool 解析是否成功
 */
bool get_var_int64(slice_t* slice, uint64_t* value);

/**
 * @brief 将64位整数编码为Varint并追加到sds字符串末尾
 * @param result 目标sds字符串
 * @param v 要追加的64位整数
 * @return sds_t 追加后的新sds字符串
 */
sds_t sds_append_var_int64(sds_t result, uint64_t v);

//数据结构 {可变32位字符串长度}{字符串内容}
/**
 * @brief 将以变长长度作为前缀的slice字符串追加到sds字符串末尾
 * @param dst 目标sds字符串
 * @param slice 要追加的slice字符串
 * @return sds_t 追加后的新sds字符串
 */
sds_t sds_append_length_prefixed_slice(sds, slice_t* slice);

/**
 * @brief 从输入切片中解析出一个以变长长度前缀结尾的slice
 * @param input 包含前缀长度和内容的输入切片（内部指针会前移）
 * @param slice 输出参数，保存解析出的字符串内容切片
 * @return bool 解析是否成功
 */
bool get_length_prefixed_slice(slice_t* input , slice_t* slice);

/**
 * @brief 移除slice前缀（将指针向后移动len个字节，长度减去len）
 * @param slice 要操作的slice
 * @param len 移除的字节数
 */
void slice_remove_prefix(slice_t* slice, size_t len);

/**
 * @brief 将slice转换为一个新的sds字符串
 * @param slice 要转换的slice
 * @return sds_t 新的sds字符串
 */
sds_t slice_to_sds(slice_t* slice);

/**
 * @brief 获取slice中指定位置的字符
 * @param slice 目标slice
 * @param n 索引位置
 * @return char 指定位置的字符
 */
char slice_operator(slice_t* slice, size_t n);

/**
 * @brief 判断slice是否为空
 * @param slice 目标slice
 * @return bool 为空返回true，否则返回false
 */
bool slice_is_empty(slice_t* slice);

/**
 * @brief 获取slice的长度
 * @param slice 目标slice
 * @return size_t 长度
 */
size_t slice_size(slice_t* slice);

/**
 * @brief 获取slice内部的字符串数据指针
 * @param slice 目标slice
 * @return char* 字符串数据指针
 */
char* slice_data(slice_t* slice);

/**
 * @brief 清空slice（置空字符串，长度设为0）
 * @param slice 目标slice
 */
void slice_clear(slice_t* slice);

/**
 * @brief 使用sds字符串初始化slice
 * @param slice 目标slice
 * @param result 用于初始化的sds字符串
 */
void slice_init_from_sds(slice_t* slice, sds_t result);

/**
 * @brief 比较两个slice的内容和长度
 * @param a 第一个slice
 * @param b 第二个slice
 * @return int 比较结果：<0(a<b), 0(a==b), >0(a>b)
 */
int slice_compare(slice_t* a, slice_t* b);

/**
 * @brief 检查slice是否以指定的字符串x作为前缀
 * @param slice 要检查的slice
 * @param x 作为前缀的slice
 * @return bool 如果是前缀则返回true，否则返回false
 */
bool slice_starts_with(slice_t* slice, slice_t* x);

#endif