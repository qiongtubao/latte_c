#include "coding.h"

sds putSdsFixed32(sds s, uint32_t value) {
    char buf[sizeof(value)];
    encodeFixed32(buf, value);
    return sdscatlen(s, buf, sizeof(value));
}

sds putSdsFixed64(sds s, uint64_t value) {
    char buf[sizeof(value)];
    encodeFixed64(buf, value);
    return sdscatlen(s, buf, sizeof(value));
}

/**
 * varint 
 * @example
 *    每byte的有效存储是7bit的，用最高的8bit位来表示是否结束（0表示结束)
 *    
 */
char* encodeVarint32(char* dst, uint32_t v) {
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
  return ptr;
}

sds putVarint32(sds s, uint32_t v) {
  char buf[5];
  char* ptr = encodeVarint32(buf, v);
  return sdscatlen(s, buf, ptr - buf);
}

const char* getVarint32PtrFallback(const char* p, const char* limit,
                                   uint32_t* value) {
  uint32_t result = 0;
  for (uint32_t shift = 0; shift <= 28 && p < limit; shift += 7) {
    uint32_t byte = *((uint8_t*)(p));
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

char* encodeVarint64(char* dst, uint64_t v) {
  static const int B = 128;
  uint8_t* ptr = (uint8_t*)(dst);
  while (v >= B) {
    *(ptr++) = v | B;
    v >>= 7;
  }
  *(ptr++) = (uint8_t)(v);
  return (char*)(ptr);
}

sds putVarint64(sds s, uint64_t v) {
  char buf[10];
  char* ptr = encodeVarint64(buf, v);
  return sdscatlen(s, buf, ptr - buf);
}


int varintLength(uint64_t v) {
  int len = 1;
  while (v >= 128) {
    v >>= 7;
    len++;
  }
  return len;
}

