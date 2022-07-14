
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "sds/sds.h"

sds putSdsFixed32(sds s, uint32_t value);
sds putSdsFixed64(sds s, uint64_t value);
sds putVarint32(sds s, uint32_t value);
sds putVarint64(sds s, uint64_t value);

bool getVarint32(sds input, uint32_t* value);
bool getVarin64(sds s, uint64_t* value);

inline uint32_t decodeFixed32(void* ptr) {
  const uint8_t* const buffer = (uint8_t*)(ptr);

  // Recent clang and gcc optimize this to a single mov / ldr instruction.
  return ((uint32_t)(buffer[0])) |
         ((uint32_t)(buffer[1]) << 8) |
         ((uint32_t)(buffer[2]) << 16) |
         ((uint32_t)(buffer[3]) << 24);
}

inline uint64_t decodeFixed64(void* ptr) {
    const uint8_t* const buffer = (uint8_t*)(ptr);

    // Recent clang and gcc optimize this to a single mov / ldr instruction.
    return ((uint32_t)(buffer[0])) |
            ((uint32_t)(buffer[1]) << 8) |
            ((uint32_t)(buffer[2]) << 16) |
            ((uint32_t)(buffer[3]) << 24) |
            ((uint64_t)(buffer[4]) << 32) |
            ((uint64_t)(buffer[5]) << 40) |
            ((uint64_t)(buffer[6]) << 48) |
            ((uint64_t)(buffer[7]) << 56);
}

inline void encodeFixed32(void* buf, uint32_t value) {
    uint8_t* uint8Buf = (uint8_t*)buf;
    uint8Buf[0] = (uint8_t)value;
    uint8Buf[1] = (uint8_t)(value >> 8);
    uint8Buf[2] = (uint8_t)(value >> 16);
    uint8Buf[3] = (uint8_t)(value >> 24);
}

inline void encodeFixed64(void* buf, uint64_t value) {
    uint8_t* uint8Buf = (uint8_t*)buf;
    uint8Buf[0] = (uint8_t)value;
    uint8Buf[1] = (uint8_t)(value >> 8);
    uint8Buf[2] = (uint8_t)(value >> 16);
    uint8Buf[3] = (uint8_t)(value >> 24);
    uint8Buf[4] = (uint8_t)(value >> 32);
    uint8Buf[5] = (uint8_t)(value >> 40);
    uint8Buf[6] = (uint8_t)(value >> 48);
    uint8Buf[7] = (uint8_t)(value >> 56);
}


// Internal routine for use by fallback path of GetVarint32Ptr
const char* getVarint32PtrFallback(const char* p, const char* limit,
                                   uint32_t* value);
inline const char* getVarint32Ptr(const char* p, const char* limit,
                                  uint32_t* value) {
    if (p < limit) {
        uint32_t result = *((uint8_t*)(p));
        if ((result & 128) == 0) {
            *value = result;
            return p + 1;
        }
    }
    return getVarint32PtrFallback(p, limit, value);
}

inline const char* getVarint64Ptr(const char* p, const char* limit, uint64_t* value) {
  uint64_t result = 0;
  for (uint32_t shift = 0; shift <= 63 && p < limit; shift += 7) {
    uint64_t byte = *(uint8_t*)(p);
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

int varintLength(uint64_t v);
sds putLengthPrefixedSlice(sds s, sds c);

