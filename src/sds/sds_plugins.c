#include "sds_plugins.h"

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


sds sdsAppendFixed32(sds result, uint32_t value) {
    char buf[sizeof(value)];
    encodeFixed32(buf, value);
    return sdscat(result, buf);
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

sds sdsAppendFixed64(sds result, uint64_t value) {
    char buf[sizeof(value)];
    encodeFixed64(buf, value);
    return sdscat(result, buf);
}

const char* getVarint32Ptr(const char* p, const char* limit,
                        uint32_t* value) {
    return "";
}