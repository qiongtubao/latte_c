#ifndef __LATTE_SDS_PLUGINS_H
#define __LATTE_SDS_PLUGINS_H

#include "sds.h"
#include <stdint.h>
#include <stdbool.h>
sds_t sdsAppendFixed32(sds_t result, uint32_t value);
sds_t sdsAppendFixed64(sds_t result, uint64_t value);

//定量32位和64位
void encodeFixed32(char* dst, uint32_t value);
uint32_t decodeFixed32(const char* ptr);
void encodeFixed64(char* dst, uint64_t value);
uint64_t decodeFixed64(char* ptr);

//变量32位和64位
//返回长度   leveldb中返回最后位置
int encodeVarint32(char* dst, uint32_t v);
typedef struct Slice {
    char* p;
    int len;
} Slice;
bool getVarint32(Slice* slice, uint32_t* value);
sds_t sdsAppendVarint32(sds_t result, uint32_t v);

int varintLength(uint64_t v);
int encodeVarint64(char* dst, uint64_t v);
bool getVarint64(Slice* slice, uint64_t* value);
sds_t sdsAppendVarint64(sds_t result, uint64_t v);

//数据结构 {可变32位字符串长度}{字符串内容}
sds_t sdsAppendLengthPrefixedSlice(sds, Slice* slice);
bool getLengthPrefixedSlice(Slice* input , Slice* slice);
//
void SliceRemovePrefix(Slice* slice, size_t len);
sds_t SliceToSds(Slice* slice);
char SliceOperator(Slice* slice, size_t n);
bool SliceIsEmpty(Slice* slice);
size_t SliceSize(Slice* slice);
char* SliceData(Slice* slice);
void SliceClear(Slice* slice);
void SliceInitSds(Slice* slice, sds_t result);
int SliceCompare(Slice* a, Slice* b);
bool SliceStartsWith(Slice* slice, Slice* x);


#endif