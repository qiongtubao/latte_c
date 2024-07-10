#ifndef LATTE_CRC_H
#define LATTE_CRC_H
#include <inttypes.h>
#include <stdio.h>

//crc16 
uint16_t crc16xmodem(const char *buf, int len);

//crc32
uint32_t crc32c(const char* buf, int len);
uint32_t crc32c_extend(uint32_t crc, const char* buf, int len);
uint32_t crc32c_mask(uint32_t);
#endif
