#include "sds_plugins.h"
#include <string.h>

// 32位定长数据编码
void encodeFixed32(char* dst, uint32_t value) {
    uint8_t* const buffer = (uint8_t*)dst;
    buffer[0] = value;
    buffer[1] = (value >> 8);
    buffer[2] = (value >> 16);
    buffer[3] = (value >> 24);
}

uint32_t decodeFixed32(const char* ptr) {
    const uint8_t* buffer = (uint8_t*)(ptr);

    return ((uint32_t)(buffer[0])) |
            ((uint32_t)(buffer[1]) << 8) |
            ((uint32_t)(buffer[2]) << 16) |
            ((uint32_t)(buffer[3]) << 24);
}


sds_t sdsAppendFixed32(sds_t result, uint32_t value) {
    char buf[sizeof(value)];
    encodeFixed32(buf, value);
    return sds_cat(result, buf);
}
//64位定长数据编码
void encodeFixed64(char* dst, uint64_t value) {
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

uint64_t decodeFixed64(char* ptr) {
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

sds_t sdsAppendFixed64(sds_t result, uint64_t value) {
    char buf[sizeof(value)];
    encodeFixed64(buf, value);
    return sds_cat(result, buf);
}

int encodeVarint32(char* dst, uint32_t v) {
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
sds_t sdsAppendVarint32(sds_t result, uint32_t v) {
    char buf[5];
    int len = encodeVarint32(buf , v);
    return sds_cat_len(result, buf, len);
}

int encodeVarint64(char* dst, uint64_t v) {
    static const int B = 128;
    uint8_t* ptr = (uint8_t*)(dst);
    while(v >= B) {
        *(ptr++) = v | B;
        v >>= 7;
    }
    *(ptr++) = (uint8_t)(v);
    return ((char*)ptr) - dst;
}
int varintLength(uint64_t v) {
    int len = 1;
    while (v >= 128) {
        v >>= 7;
        len++;
    }
    return len;
}

sds_t sdsAppendVarint64(sds_t result, uint64_t v) {
    char buf[10];
    int len = encodeVarint64(buf , v);
    return sds_cat_len(result, buf, len);
}

char* GetVarint32PtrFallback(const char* p, const char* limit,
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
        return (const char*)(p);
        }
    }
    return NULL;
}
char* getVarint32Ptr(char* p, char* limit, uint32_t* v) {
    if (p < limit) {
        uint32_t result = *((const uint8_t*)(p));
        if ((result & 128) == 0) {
            *v = result;
            return p + 1;
        }
    }
    return GetVarint32PtrFallback(p, limit, v);
}

//可变长度，返回结束时长度
bool getVarint32(Slice* slice, uint32_t* v) {
    const char* p = slice->p;
    const char* limit = p + slice->len;
    const char* q = getVarint32Ptr(p, limit, v);
    if (q == NULL) {
        return false;
    } else {
        slice->len -= q - p;
        slice->p = q;
        return true;
    }
}

const char* getVarint64Ptr(const char* p, const char* limit, uint64_t* value) {
  uint64_t result = 0;
  for (uint32_t shift = 0; shift <= 63 && p < limit; shift += 7) {
    uint64_t byte = *((const uint8_t*)(p));
    p++;
    if (byte & 128) {
      // More bytes are present
      result |= ((byte & 127) << shift);
    } else {
      result |= (byte << shift);
      *value = result;
      return (const char*)(p);
    }
  }
  return NULL;
}

//字符串长度
bool getVarint64(Slice* slice, uint64_t* value) {
  const char* p = slice->p;
  const char* limit = p + slice->len;
  //动态的64bit   limit会返回最后的位置
  const char* q = getVarint64Ptr(p, limit, value);
  if (q == NULL) {
    return false;
  } else {
    slice->len -= q - p;
    slice->p = q;
    return true;
  }
}

sds_t sdsAppendLengthPrefixedSlice(sds_t dst, Slice* slice) {
    dst = sdsAppendVarint32(dst, slice->len);
    return sds_cat_len(dst, slice->p, slice->len);
}

//这里不复制数据
bool getLengthPrefixedSlice(Slice* input , Slice* result) {
  uint32_t len;
  if (getVarint32(input, &len) && input->len >= len) {
    result->p = input->p;
    result->len = len;
    SliceRemovePrefix(input, len);
    return true;
  } else {
    return false;
  }
}

//
void SliceRemovePrefix(Slice* slice, size_t len) {
  slice->p = slice->p + len;
  slice->len = slice->len - len;
}

sds_t SliceToSds(Slice* slice) {
  return sds_new_len(slice->p, slice->len);
}

char SliceOperator(Slice* slice, size_t n) {
  return slice->p[n];
}

bool SliceIsEmpty(Slice* slice) {
  return slice->len == 0;
}

size_t SliceSize(Slice* slice) {
  return slice->len;
}
char* SliceData(Slice* slice) {
  return slice->p;
}

void SliceClear(Slice* slice) {
  slice->p = "";
  slice->len = 0;
}

void SliceInitSds(Slice* slice, sds_t result) {
  slice->p = result;
  slice->len = sds_len(result);
}

int SliceCompare(Slice* a, Slice* b) {
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

bool SliceStartsWith(Slice* slice, Slice* x) {
  return ((slice->len >= x->len) && (memcmp(slice->p, x->p, x->len) == 0));
}