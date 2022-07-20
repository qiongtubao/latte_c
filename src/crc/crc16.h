#include <stdint.h>
#include <stdlib.h>


typedef uint16_t (*crcfn16)(uint16_t, const void *, const uint64_t);
//default crcfn16
uint16_t crc16(const char *buf, int len);

void crcspeed16little_init(crcfn16 fn, uint16_t table[8][256]);
void crcspeed16big_init(crcfn16 fn, uint16_t table[8][256]);
void crcspeed16native_init(crcfn16 fn, uint16_t table[8][256]);

uint16_t crcspeed16little(uint16_t table[8][256], uint16_t crc, void *buf,
                          size_t len);
uint16_t crcspeed16big(uint16_t table[8][256], uint16_t crc, void *buf,
                       size_t len);
uint16_t crcspeed16native(uint16_t table[8][256], uint16_t crc, void *buf,
                          size_t len);