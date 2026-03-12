#include "sds_plugins.h"

#include <string.h>

/**
 * @brief 将32位无符号整数编码为定长字节序列（小端序）
 * @param dst 目标缓冲区，至少需要4字节
 * @param value 要编码的32位无符号整数
 */
// 32位定长数据编码
void encode_fixed32(char* dst, uint32_t value) {
    uint8_t* const buffer = (uint8_t*)dst;
    buffer[0] = value;
    buffer[1] = (value >> 8);
    buffer[2] = (value >> 16);
    buffer[3] = (value >> 24);
}

/**
 * @brief 从字节序列解码出32位无符号整数（小端序）
 * @param ptr 指向字节序列的指针
 * @return uint32_t 解码出的32位无符号整数
 */
uint32_t decode_fixed32(const char* ptr) {
    const uint8_t* buffer = (uint8_t*)(ptr);

    return ((uint32_t)(buffer[0])) |
            ((uint32_t)(buffer[1]) << 8) |
            ((uint32_t)(buffer[2]) << 16) |
            ((uint32_t)(buffer[3]) << 24);
}


/**
 * @brief 将32位无符号整数以定长方式追加到sds字符串末尾
 * @param result 目标sds字符串
 * @param value 要追加的32位无符号整数
 * @return sds_t 追加后的新sds字符串
 */
sds_t sds_append_fixed32(sds_t result, uint32_t value) {
    char buf[sizeof(value)];
    encode_fixed32(buf, value);
    return sds_cat(result, buf);
}

/**
 * @brief 将64位无符号整数编码为定长字节序列（小端序）
 * @param dst 目标缓冲区，至少需要8字节
 * @param value 要编码的64位无符号整数
 */
//64位定长数据编码
void encode_fixed64(char* dst, uint64_t value) {
    uint8_t* const buffer = (uint8_t*)(dst);
    buffer[0] = (uint8_t)(value);
    buffer[1] = (uint8_t)(value >> 8);
    buffer[2] = (uint8_t)(value >> 16);
    buffer[3] = (uint8_t)(value >> 24);
    buffer[4] = (uint8_t)(value >> 32);
    buffer[5] = (uint8_t)(value >> 40);
    buffer[6] = (uint8_t)(value >> 48);
    buffer[7] = (uint8_t)(value >> 56);
}

/**
 * @brief 从字节序列解码出64位无符号整数（小端序）
 * @param ptr 指向字节序列的指针
 * @return uint64_t 解码出的64位无符号整数
 */
uint64_t decode_fixed64(char* ptr) {
    const uint8_t* buffer = (uint8_t*)ptr;
    return (uint64_t)(buffer[0]) |
        (((uint64_t)buffer[1]) << 8) |
        (((uint64_t)buffer[2]) << 16) |
        (((uint64_t)buffer[3]) << 24) |
        (((uint64_t)buffer[4]) << 32) |
        (((uint64_t)buffer[5]) << 40) |
        (((uint64_t)buffer[6]) << 48) |
        (((uint64_t)buffer[7]) << 56);
}

/**
 * @brief 将64位无符号整数以定长方式追加到sds字符串末尾
 * @param result 目标sds字符串
 * @param value 要追加的64位无符号整数
 * @return sds_t 追加后的新sds字符串
 */
sds_t sds_append_fixed64(sds_t result, uint64_t value) {
    char buf[sizeof(value)];
    encode_fixed64(buf, value);
    return sds_cat(result, buf);
}

/**
 * @brief 将32位无符号整数编码为变长字节序列（Varint）
 * @param dst 目标缓冲区，至少需要5字节
 * @param v 要编码的32位无符号整数
 * @return int 编码后占用的字节数
 */
int encode_var_int32(char* dst, uint32_t v) {
    uint8_t* ptr = (uint8_t*)dst;
    static const int B = 128;
    if (v < (1 << 7)) {
    *(ptr++) = v;
  } else if (v < (1 << 14)) {
    *(ptr++) = v | B;
    *(ptr++) = v >> 7;
  } else if (v < (1 << 21)) {
    *(ptr++) = v | B;
    *(ptr++) = (v >> 7) | B;
    *(ptr++) = v >> 14;
  } else if (v < (1 << 28)) {
    *(ptr++) = v | B;
    *(ptr++) = (v >> 7) | B;
    *(ptr++) = (v >> 14) | B;
    *(ptr++) = v >> 21;
  } else {
    *(ptr++) = v | B;
    *(ptr++) = (v >> 7) | B;
    *(ptr++) = (v >> 14) | B;
    *(ptr++) = (v >> 21) | B;
    *(ptr++) = v >> 28;
  }
  return  ((char*)ptr) - dst;
}

/**
 * @brief 将32位整数编码为Varint并追加到sds字符串末尾
 * @param result 目标sds字符串
 * @param v 要追加的32位整数
 * @return sds_t 追加后的新sds字符串
 */
sds_t sds_append_var_int32(sds_t result, uint32_t v) {
    char buf[5];
    int len = encode_var_int32(buf , v);
    return sds_cat_len(result, buf, len);
}

/**
 * @brief 将64位无符号整数编码为变长字节序列（Varint）
 * @param dst 目标缓冲区，至少需要10字节
 * @param v 要编码的64位无符号整数
 * @return int 编码后占用的字节数
 */
int encode_var_int64(char* dst, uint64_t v) {
    static const uint32_t B = 128;
    uint8_t* ptr = (uint8_t*)(dst);
    while(v >= B) {
        *(ptr++) = v | B;
        v >>= 7;
    }
    *(ptr++) = (uint8_t)(v);
    return ((char*)ptr) - dst;
}

/**
 * @brief 计算64位整数编码为Varint后所需的字节长度
 * @param v 64位整数
 * @return int 所需的字节数
 */
int var_int_length(uint64_t v) {
    int len = 1;
    while (v >= 128) {
        v >>= 7;
        len++;
    }
    return len;
}

/**
 * @brief 将64位整数编码为Varint并追加到sds字符串末尾
 * @param result 目标sds字符串
 * @param v 要追加的64位整数
 * @return sds_t 追加后的新sds字符串
 */
sds_t sds_append_var_int64(sds_t result, uint64_t v) {
    char buf[10];
    int len = encode_var_int64(buf , v);
    return sds_cat_len(result, buf, len);
}

/**
 * @brief 解析变长32位整数的后备函数（处理超过1字节的情况）
 * @param p 输入指针
 * @param limit 缓冲区结束边界
 * @param value 输出参数，存放解析出的结果
 * @return char* 解析后的新指针位置，如果解析失败返回NULL
 */
char* get_var_int32_ptr_fallback(const char* p, const char* limit,
                                   uint32_t* value) {
    uint32_t result = 0;
    for (uint32_t shift = 0; shift <= 28 && p < limit; shift += 7) {
        uint32_t byte = *((const uint8_t*)(p));
        p++;
        if (byte & 128) {
        // More bytes are present
        result |= ((byte & 127) << shift);
        } else {
        result |= (byte << shift);
        *value = result;
        return (char*)(p);
        }
    }
    return NULL;
}

/**
 * @brief 解析变长32位整数（先尝试快速解析单字节情况）
 * @param p 输入指针
 * @param limit 缓冲区结束边界
 * @param v 输出参数，存放解析出的结果
 * @return char* 解析后的新指针位置，如果解析失败返回NULL
 */
char* get_var_int32_ptr(char* p, char* limit, uint32_t* v) {
    if (p < limit) {
        uint32_t result = *((const uint8_t*)(p));
        if ((result & 128) == 0) {
            *v = result;
            return p + 1;
        }
    }
    return get_var_int32_ptr_fallback(p, limit, v);
}

/**
 * @brief 从slice中解析出变长32位整数，并移动slice指针
 * @param slice 输入数据的切片，解析后内部指针会前移
 * @param v 输出参数，用于存放解析出的32位整数
 * @return bool 解析是否成功
 */
//可变长度，返回结束时长度
bool get_var_int32(slice_t* slice, uint32_t* v) {
    char* p = slice->p;
    char* limit = p + slice->len;
    char* q = get_var_int32_ptr(p, limit, v);
    if (q == NULL) {
        return false;
    } else {
        slice->len -= q - p;
        slice->p = q;
        return true;
    }
}

/**
 * @brief 解析变长64位整数
 * @param p 输入指针
 * @param limit 缓冲区结束边界
 * @param value 输出参数，存放解析出的结果
 * @return char* 解析后的新指针位置，如果解析失败返回NULL
 */
char* get_var_int64_ptr(const char* p, const char* limit, uint64_t* value) {
  uint64_t result = 0;
  for (uint32_t shift = 0; shift <= 63 && p < limit; shift += 7) {
    uint64_t byte = *((uint8_t*)(p));
    p++;
    if (byte & 128) {
      // More bytes are present
      result |= ((byte & 127) << shift);
    } else {
      result |= (byte << shift);
      *value = result;
      return (char*)(p);
    }
  }
  return NULL;
}

/**
 * @brief 从slice中解析出变长64位整数，并移动slice指针
 * @param slice 输入数据的切片，解析后内部指针会前移
 * @param value 输出参数，用于存放解析出的64位整数
 * @return bool 解析是否成功
 */
//字符串长度
bool get_var_int64(slice_t* slice, uint64_t* value) {
  const char* p = slice->p;
  const char* limit = p + slice->len;
  //动态的64bit   limit会返回最后的位置
  char* q = get_var_int64_ptr(p, limit, value);
  if (q == NULL) {
    return false;
  } else {
    slice->len -= q - p;
    slice->p = q;
    return true;
  }
}

/**
 * @brief 将以变长长度作为前缀的slice字符串追加到sds字符串末尾
 * @param dst 目标sds字符串
 * @param slice 要追加的slice字符串
 * @return sds_t 追加后的新sds字符串
 */
sds_t sds_append_length_prefixed_slice(sds_t dst, slice_t* slice) {
    dst = sds_append_var_int32(dst, slice->len);
    return sds_cat_len(dst, slice->p, slice->len);
}

/**
 * @brief 从输入切片中解析出一个以变长长度前缀结尾的slice
 * @param input 包含前缀长度和内容的输入切片（内部指针会前移）
 * @param result 输出参数，保存解析出的字符串内容切片
 * @return bool 解析是否成功
 */
//这里不复制数据
bool get_length_prefixed_slice(slice_t* input , slice_t* result) {
  uint32_t len;
  if (get_var_int32(input, &len) && input->len >= (int)len) {
    result->p = input->p;
    result->len = len;
    slice_remove_prefix(input, len);
    return true;
  } else {
    return false;
  }
}

/**
 * @brief 移除slice前缀（将指针向后移动len个字节，长度减去len）
 * @param slice 要操作的slice
 * @param len 移除的字节数
 */
//
void slice_remove_prefix(slice_t* slice, size_t len) {
  slice->p = slice->p + len;
  slice->len = slice->len - len;
}

/**
 * @brief 将slice转换为一个新的sds字符串
 * @param slice 要转换的slice
 * @return sds_t 新的sds字符串
 */
sds_t slice_to_sds(slice_t* slice) {
  return sds_new_len(slice->p, slice->len);
}

/**
 * @brief 获取slice中指定位置的字符
 * @param slice 目标slice
 * @param n 索引位置
 * @return char 指定位置的字符
 */
char slice_operator(slice_t* slice, size_t n) {
  return slice->p[n];
}

/**
 * @brief 判断slice是否为空
 * @param slice 目标slice
 * @return bool 为空返回true，否则返回false
 */
bool slice_is_empty(slice_t* slice) {
  return slice->len == 0;
}

/**
 * @brief 获取slice的长度
 * @param slice 目标slice
 * @return size_t 长度
 */
size_t slice_size(slice_t* slice) {
  return slice->len;
}

/**
 * @brief 获取slice内部的字符串数据指针
 * @param slice 目标slice
 * @return char* 字符串数据指针
 */
char* slice_data(slice_t* slice) {
  return slice->p;
}

/**
 * @brief 清空slice（置空字符串，长度设为0）
 * @param slice 目标slice
 */
void slice_clear(slice_t* slice) {
  slice->p = "";
  slice->len = 0;
}

/**
 * @brief 使用sds字符串初始化slice
 * @param slice 目标slice
 * @param result 用于初始化的sds字符串
 */
void slice_init_from_sds(slice_t* slice, sds_t result) {
  slice->p = result;
  slice->len = sds_len(result);
}

/**
 * @brief 比较两个slice的内容和长度
 * @param a 第一个slice
 * @param b 第二个slice
 * @return int 比较结果：<0(a<b), 0(a==b), >0(a>b)
 */
int slice_compare(slice_t* a, slice_t* b) {
  const size_t min_len = (a->len < b->len) ? a->len : b->len;
  int r = memcmp(a->p, b->p, min_len);
  if (r == 0) {
    if (a->len < b->len)
      r = -1;
    else if (a->len > b->len)
      r = +1;
  }
  return r;
}

/**
 * @brief 检查slice是否以指定的字符串x作为前缀
 * @param slice 要检查的slice
 * @param x 作为前缀的slice
 * @return bool 如果是前缀则返回true，否则返回false
 */
bool slice_starts_with(slice_t* slice, slice_t* x) {
  return ((slice->len >= x->len) && (memcmp(slice->p, x->p, x->len) == 0));
}