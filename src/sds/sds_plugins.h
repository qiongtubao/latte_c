#ifndef __LATTE_SDS_PLUGINS_H
#define __LATTE_SDS_PLUGINS_H

#include "sds.h"
#include <stdint.h>
#include <stdbool.h>
sds sdsAppendFixed32(sds result, uint32_t value);
sds sdsAppendFixed64(sds result, uint64_t value);

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
sds sdsAppendVarint32(sds result, uint32_t v);

int varintLength(uint64_t v);
int encodeVarint64(char* dst, uint64_t v);
bool getVarint64(Slice* slice, uint64_t* value);
sds sdsAppendVarint64(sds result, uint64_t v);

#endif