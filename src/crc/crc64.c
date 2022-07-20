#include "crc64.h"
#include "crc_util.h"
static uint64_t crc64_table[8][256] = {{0}};
#define POLY UINT64_C(0xad93d23594c935a9)

/**
 * Reflect all bits of a \a data word of \a data_len bytes.
 *
 * \param data         The data word to be reflected.
 * \param data_len     The width of \a data expressed in number of bits.
 * \return             The reflected data.
 *****************************************************************************/
static inline uint_fast64_t crc_reflect(uint_fast64_t data, size_t data_len) {
    uint_fast64_t ret = data & 0x01;

    for (size_t i = 1; i < data_len; i++) {
        data >>= 1;
        ret = (ret << 1) | (data & 0x01);
    }

    return ret;
}
/**
 *  Update the crc value with new data.
 *
 * \param crc      The current crc value.
 * \param data     Pointer to a buffer of \a data_len bytes.
 * \param data_len Number of bytes in the \a data buffer.
 * \return         The updated crc value.
 ******************************************************************************/
uint64_t _crc64(uint_fast64_t crc, const void *in_data, const uint64_t len) {
    const uint8_t *data = in_data;
    unsigned long long bit;

    for (uint64_t offset = 0; offset < len; offset++) {
        uint8_t c = data[offset];
        for (uint_fast8_t i = 0x01; i & 0xff; i <<= 1) {
            bit = crc & 0x8000000000000000;
            if (c & i) {
                bit = !bit;
            }

            crc <<= 1;
            if (bit) {
                crc ^= POLY;
            }
        }

        crc &= 0xffffffffffffffff;
    }

    crc = crc & 0xffffffffffffffff;
    return crc_reflect(crc, 64) ^ 0x0000000000000000;
}




/* Fill in a CRC constants table. */
void crcspeed64little_init(crcfn64 crcfn, uint64_t table[8][256]) {
    uint64_t crc;

    /* generate CRCs for all single byte sequences */
    for (int n = 0; n < 256; n++) {
        unsigned char v = n;
        table[0][n] = crcfn(0, &v, 1);
    }

    /* generate nested CRC table for future slice-by-8 lookup */
    for (int n = 0; n < 256; n++) {
        crc = table[0][n];
        for (int k = 1; k < 8; k++) {
            crc = table[0][crc & 0xff] ^ (crc >> 8);
            table[k][n] = crc;
        }
    }
}

/* This function is called once to initialize the CRC table for use on a
   big-endian architecture. */
void crcspeed64big_init(crcfn64 fn, uint64_t big_table[8][256]) {
    /* Create the little endian table then reverse all the entries. */
    crcspeed64little_init(fn, big_table);
    for (int k = 0; k < 8; k++) {
        for (int n = 0; n < 256; n++) {
            big_table[k][n] = rev8(big_table[k][n]);
        }
    }
}

/* Initialize CRC lookup table in architecture-dependent manner. */
void crcspeed64native_init(crcfn64 fn, uint64_t table[8][256]) {
    uint64_t n = 1;

    *(char *)&n ? crcspeed64little_init(fn, table)
                : crcspeed64big_init(fn, table);
}

/* Initializes the 16KB lookup tables. */
void crc64_init(void) {
    crcspeed64native_init(_crc64, crc64_table);
}



/* Calculate a non-inverted CRC multiple bytes at a time on a little-endian
 * architecture. If you need inverted CRC, invert *before* calling and invert
 * *after* calling.
 * 64 bit crc = process 8 bytes at once;
 */
uint64_t crcspeed64little(uint64_t little_table[8][256], uint64_t crc,
                          void *buf, size_t len) {
    unsigned char *next = buf;

    /* process individual bytes until we reach an 8-byte aligned pointer */
    while (len && ((uintptr_t)next & 7) != 0) {
        crc = little_table[0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
        len--;
    }

    /* fast middle processing, 8 bytes (aligned!) per loop */
    while (len >= 8) {
        crc ^= *(uint64_t *)next;
        crc = little_table[7][crc & 0xff] ^
              little_table[6][(crc >> 8) & 0xff] ^
              little_table[5][(crc >> 16) & 0xff] ^
              little_table[4][(crc >> 24) & 0xff] ^
              little_table[3][(crc >> 32) & 0xff] ^
              little_table[2][(crc >> 40) & 0xff] ^
              little_table[1][(crc >> 48) & 0xff] ^
              little_table[0][crc >> 56];
        next += 8;
        len -= 8;
    }

    /* process remaining bytes (can't be larger than 8) */
    while (len) {
        crc = little_table[0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
        len--;
    }

    return crc;
}

/* Calculate a non-inverted CRC eight bytes at a time on a big-endian
 * architecture.
 */
uint64_t crcspeed64big(uint64_t big_table[8][256], uint64_t crc, void *buf,
                       size_t len) {
    unsigned char *next = buf;

    crc = rev8(crc);
    while (len && ((uintptr_t)next & 7) != 0) {
        crc = big_table[0][(crc >> 56) ^ *next++] ^ (crc << 8);
        len--;
    }

    while (len >= 8) {
        crc ^= *(uint64_t *)next;
        crc = big_table[0][crc & 0xff] ^
              big_table[1][(crc >> 8) & 0xff] ^
              big_table[2][(crc >> 16) & 0xff] ^
              big_table[3][(crc >> 24) & 0xff] ^
              big_table[4][(crc >> 32) & 0xff] ^
              big_table[5][(crc >> 40) & 0xff] ^
              big_table[6][(crc >> 48) & 0xff] ^
              big_table[7][crc >> 56];
        next += 8;
        len -= 8;
    }

    while (len) {
        crc = big_table[0][(crc >> 56) ^ *next++] ^ (crc << 8);
        len--;
    }

    return rev8(crc);
}

/* Return the CRC of buf[0..len-1] with initial crc, processing eight bytes
   at a time using passed-in lookup table.
   This selects one of two routines depending on the endianess of
   the architecture. */
uint64_t crcspeed64native(uint64_t table[8][256], uint64_t crc, void *buf,
                          size_t len) {
    uint64_t n = 1;

    return *(char *)&n ? crcspeed64little(table, crc, buf, len)
                       : crcspeed64big(table, crc, buf, len);
}

/* Compute crc64 */
uint64_t crc64(uint64_t crc, const unsigned char *s, uint64_t l) {
    return crcspeed64native(crc64_table, crc, (void *) s, l);
}
