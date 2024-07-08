#ifndef __LATTE_SDS_PLUGINS_H
#define __LATTE_SDS_PLUGINS_H

#include "sds.h"
sds sdsAppendFixed32(sds result, uint32_t value);
sds sdsAppendFixed64(sds result, uint64_t value);


void encodeFixed32(char* dst, uint32_t value);
uint32_t decodeFixed32(const char* ptr);
void encodeFixed64(char* dst, uint64_t value);
uint64_t decodeFixed64(char* ptr);


#endif