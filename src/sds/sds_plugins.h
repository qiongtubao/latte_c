#ifndef __LATTE_SDS_PLUGINS_H
#define __LATTE_SDS_PLUGINS_H

#include "sds.h"
#include <stdint.h>
#include <stdbool.h>
sds_t sds_append_fixed32(sds_t result, uint32_t value);
sds_t sds_append_fixed64(sds_t result, uint64_t value);

//定量32位和64位
void encode_fixed32(char* dst, uint32_t value);
uint32_t decode_fixed32(const char* ptr);
void encode_fixed64(char* dst, uint64_t value);
uint64_t decode_fixed64(char* ptr);

//变量32位和64位
//返回长度   leveldb中返回最后位置
int encode_var_int32(char* dst, uint32_t v);
typedef struct slice_t {
    char* p;
    int len;
} slice_t;
bool get_var_int32(slice_t* slice, uint32_t* value);
sds_t sds_append_var_int32(sds_t result, uint32_t v);

int var_int_length(uint64_t v);
int encode_var_int64(char* dst, uint64_t v);
bool get_var_int64(slice_t* slice, uint64_t* value);
sds_t sds_append_var_int64(sds_t result, uint64_t v);

//数据结构 {可变32位字符串长度}{字符串内容}
sds_t sds_append_length_prefixed_slice(sds, slice_t* slice);
bool get_length_prefixed_slice(slice_t* input , slice_t* slice);
//
void slice_remove_prefix(slice_t* slice, size_t len);
sds_t slice_to_sds(slice_t* slice);
char slice_operator(slice_t* slice, size_t n);
bool slice_is_empty(slice_t* slice);
size_t slice_size(slice_t* slice);
char* slice_data(slice_t* slice);
void slice_clear(slice_t* slice);
void slice_init_from_sds(slice_t* slice, sds_t result);
int slice_compare(slice_t* a, slice_t* b);
bool slice_starts_with(slice_t* slice, slice_t* x);


#endif