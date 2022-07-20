#include <stdint.h>
#include <stdlib.h>


typedef uint64_t (*crcfn64)(uint64_t, const void *, const uint64_t);
//default crc64
uint64_t crc64(uint64_t crc, const unsigned char *s, uint64_t l);
void crc64_init();

/* CRC-64 */
void crcspeed64little_init(crcfn64 fn, uint64_t table[8][256]);
void crcspeed64big_init(crcfn64 fn, uint64_t table[8][256]);
void crcspeed64native_init(crcfn64 fn, uint64_t table[8][256]);

uint64_t crcspeed64little(uint64_t table[8][256], uint64_t crc, void *buf,
                          size_t len);
uint64_t crcspeed64big(uint64_t table[8][256], uint64_t crc, void *buf,
                       size_t len);
uint64_t crcspeed64native(uint64_t table[8][256], uint64_t crc, void *buf,
                          size_t len);