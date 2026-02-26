





/* Copyright (C) 2013 Mark Adler
 * Copyright (C) 2019-2024 Josiah Carlson
 * Portions originally from: crc64.c Version 1.4  16 Dec 2013  Mark Adler
 * Modifications by Josiah Carlson <josiah.carlson@gmail.com>
 *   - Added implementation variations with sample timings for gf_matrix_times*()
 *   - Most folks would be best using gf2_matrix_times_vec or
 *	   gf2_matrix_times_vec2, unless some processor does AVX2 fast.
 *   - This is the implementation of the MERGE_CRC macro defined in
 *     crcspeed.c (which calls crc_combine()), and is a specialization of the
 *     generic crc_combine() (and related from the 2013 edition of Mark Adler's
 *     crc64.c)) for the sake of clarity and performance.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
	 claim that you wrote the original software. If you use this software
	 in a product, an acknowledgment in the product documentation would be
	 appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
	 misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler
  madler@alumni.caltech.edu
*/

#include "crc64.h"

#define STATIC_ASSERT(VVV) do {int test = 1 / (VVV);test++;} while (0)

#if !((defined(__i386__) || defined(__X86_64__)))

/* This cuts 40% of the time vs bit-by-bit. */

uint64_t gf2_matrix_times_switch(uint64_t *mat, uint64_t vec) {
	/*
	 * Without using any vector math, this handles 4 bits at a time,
	 * and saves 40+% of the time compared to the bit-by-bit version. Use if you
	 * have no vector compile option available to you. With cache, we see:
	 * E5-2670 ~1-2us to extend ~1 meg 64 bit hash
	 */
	uint64_t sum;

	sum = 0;
	while (vec) {
		/* reversing the case order is ~10% slower on Xeon E5-2670 */
		switch (vec & 15) {
		case 15:
			sum ^= *mat ^ *(mat+1) ^ *(mat+2) ^ *(mat+3);
			break;
		case 14:
			sum ^= *(mat+1) ^ *(mat+2) ^ *(mat+3);
			break;
		case 13:
			sum ^= *mat ^ *(mat+2) ^ *(mat+3);
			break;
		case 12:
			sum ^= *(mat+2) ^ *(mat+3);
			break;
		case 11:
			sum ^= *mat ^ *(mat+1) ^ *(mat+3);
			break;
		case 10:
			sum ^= *(mat+1) ^ *(mat+3);
			break;
		case 9:
			sum ^= *mat ^ *(mat+3);
			break;
		case 8:
			sum ^= *(mat+3);
			break;
		case 7:
			sum ^= *mat ^ *(mat+1) ^ *(mat+2);
			break;
		case 6:
			sum ^= *(mat+1) ^ *(mat+2);
			break;
		case 5:
			sum ^= *mat ^ *(mat+2);
			break;
		case 4:
			sum ^= *(mat+2);
			break;
		case 3:
			sum ^= *mat ^ *(mat+1);
			break;
		case 2:
			sum ^= *(mat+1);
			break;
		case 1:
			sum ^= *mat;
			break;
		default:
			break;
		}
		vec >>= 4;
		mat += 4;
	}
	return sum;
}

#define CRC_MULTIPLY gf2_matrix_times_switch

#else

/*
	Warning: here there be dragons involving vector math, and macros to save us
	from repeating the same information over and over.
*/

uint64_t gf2_matrix_times_vec2(uint64_t *mat, uint64_t vec) {
	/*
	 * Uses xmm registers on x86, works basically everywhere fast, doing
	 * cycles of movqda, mov, shr, pand, and, pxor, at least on gcc 8.
	 * Is 9-11x faster than original.
	 * E5-2670 ~29us to extend ~1 meg 64 bit hash
	 * i3-8130U ~22us to extend ~1 meg 64 bit hash
	 */
	v2uq sum = {0, 0},
		*mv2 = (v2uq*)mat;
	/* this table allows us to eliminate conditions during gf2_matrix_times_vec2() */
	static v2uq masks2[4] = {
		{0,0},
		{-1,0},
		{0,-1},
		{-1,-1},
	};

	/* Almost as beautiful as gf2_matrix_times_vec, but only half as many
	 * bits per step, so we need 2 per chunk4 operation. Faster in my tests. */

#define DO_CHUNK4() \
		sum ^= (*mv2++) & masks2[vec & 3]; \
		vec >>= 2; \
		sum ^= (*mv2++) & masks2[vec & 3]; \
		vec >>= 2

#define DO_CHUNK16() \
		DO_CHUNK4(); \
		DO_CHUNK4(); \
		DO_CHUNK4(); \
		DO_CHUNK4()

	DO_CHUNK16();
	DO_CHUNK16();
	DO_CHUNK16();
	DO_CHUNK16();

	STATIC_ASSERT(sizeof(uint64_t) == 8);
	STATIC_ASSERT(sizeof(long long unsigned int) == 8);
	return sum[0] ^ sum[1];
}

#undef DO_CHUNK16
#undef DO_CHUNK4

#define CRC_MULTIPLY gf2_matrix_times_vec2
#endif

static void gf2_matrix_square(uint64_t *square, uint64_t *mat, uint8_t dim) {
	unsigned n;

	for (n = 0; n < dim; n++)
		square[n] = CRC_MULTIPLY(mat, mat[n]);
}

/* Turns out our Redis / Jones CRC cycles at this point, so we can support
 * more than 64 bits of extension if we want. Trivially. */
static uint64_t combine_cache[64][64];

/* Mark Adler has some amazing updates to crc.c in his crcany repository. I
 * like static caches, and not worrying about finding cycles generally. We are
 * okay to spend the 32k of memory here, leaving the algorithm unchanged from
 * as it was a decade ago, and be happy that it costs <200 microseconds to
 * init, and that subsequent calls to the combine function take under 100
 * nanoseconds. We also note that the crcany/crc.c code applies to any CRC, and
 * we are currently targeting one: Jones CRC64.
 */

void init_combine_cache(uint64_t poly, uint8_t dim) {
	unsigned n, cache_num = 0;
	combine_cache[1][0] = poly;
	int prev = 1;
	uint64_t row = 1;
	for (n = 1; n < dim; n++)
	{
		combine_cache[1][n] = row;
		row <<= 1;
	}

	gf2_matrix_square(combine_cache[0], combine_cache[1], dim);
	gf2_matrix_square(combine_cache[1], combine_cache[0], dim);

	/* do/while to overwrite the first two layers, they are not used, but are
	 * re-generated in the last two layers for the Redis polynomial */
	do {
		gf2_matrix_square(combine_cache[cache_num], combine_cache[cache_num + prev], dim);
		prev = -1;
	} while (++cache_num < 64);
}

/* Return the CRC-64 of two sequential blocks, where crc1 is the CRC-64 of the
 * first block, crc2 is the CRC-64 of the second block, and len2 is the length
 * of the second block.
 *
 * If you want reflections on your CRCs; do them outside before / after.
 * WARNING: if you enable USE_STATIC_COMBINE_CACHE to make this fast, you MUST
 * ALWAYS USE THE SAME POLYNOMIAL, otherwise you will get the wrong results.
 * You MAY bzero() the even/odd static arrays, which will induce a re-cache on
 * next call as a work-around, but ... maybe just parameterize the cached
 * models at that point like Mark Adler does in modern crcany/crc.c .
 */
uint64_t crc64_combine(uint64_t crc1, uint64_t crc2, uintmax_t len2, uint64_t poly, uint8_t dim) {
	/* degenerate case */
	if (len2 == 0)
		return crc1;

	unsigned cache_num = 0;
	if (combine_cache[0][0] == 0) {
		init_combine_cache(poly, dim);
	}

	/* apply len2 zeros to crc1 (first square will put the operator for one
	   zero byte, eight zero bits, in even) */
	do
	{
		/* apply zeros operator for this bit of len2 */
		if (len2 & 1)
			crc1 = CRC_MULTIPLY(combine_cache[cache_num], crc1);
		len2 >>= 1;
		cache_num = (cache_num + 1) & 63;
		/* if no more bits set, then done */
	} while (len2 != 0);

	/* return combined crc */
	crc1 ^= crc2;
	return crc1;
}



#define CRC64_LEN_MASK UINT64_C(0x7ffffffffffffff8)
#define CRC64_REVERSED_POLY UINT64_C(0x95ac9329ac4bc9b5)

/* Fill in a CRC constants table. */
void crcspeed64little_init(crcfn64 crcfn, uint64_t table[8][256]) {
    uint64_t crc;

    /* generate CRCs for all single byte sequences */
    for (int n = 0; n < 256; n++) {
        unsigned char v = n;
        table[0][n] = crcfn(0, &v, 1);
    }

    /* generate nested CRC table for future slice-by-8/16/24+ lookup */
    for (int n = 0; n < 256; n++) {
        crc = table[0][n];
        for (int k = 1; k < 8; k++) {
            crc = table[0][crc & 0xff] ^ (crc >> 8);
            table[k][n] = crc;
        }
    }
#if USE_STATIC_COMBINE_CACHE
    /* initialize combine cache for CRC stapling for slice-by 16/24+ */
    init_combine_cache(CRC64_REVERSED_POLY, 64);
#endif
}


/* Reverse the bytes in a 64-bit word. */
static inline uint64_t rev8(uint64_t a) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(a);
#else
    uint64_t m;

    m = UINT64_C(0xff00ff00ff00ff);
    a = ((a >> 8) & m) | (a & m) << 8;
    m = UINT64_C(0xffff0000ffff);
    a = ((a >> 16) & m) | (a & m) << 16;
    return a >> 32 | a << 32;
#endif
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



/* Note: doing all of our crc/next modifications *before* the crc table
 * references is an absolute speedup on all CPUs tested. So... keep these
 * macros separate.
 */

#define DO_8_1(crc, next)                            \
    crc ^= *(uint64_t *)next;                        \
    next += 8

#define DO_8_2(crc)                                  \
    crc = little_table[7][(uint8_t)crc] ^            \
             little_table[6][(uint8_t)(crc >> 8)] ^  \
             little_table[5][(uint8_t)(crc >> 16)] ^ \
             little_table[4][(uint8_t)(crc >> 24)] ^ \
             little_table[3][(uint8_t)(crc >> 32)] ^ \
             little_table[2][(uint8_t)(crc >> 40)] ^ \
             little_table[1][(uint8_t)(crc >> 48)] ^ \
             little_table[0][crc >> 56]

#define CRC64_SPLIT(div) \
    olen = len; \
    next2 = next1 + ((len / div) & CRC64_LEN_MASK); \
    len = (next2 - next1)

#define MERGE_CRC(crcn) \
    crc1 = crc64_combine(crc1, crcn, next2 - next1, CRC64_REVERSED_POLY, 64)

#define MERGE_END(last, DIV) \
    len = olen - ((next2 - next1) * DIV); \
    next1 = last

/* Variables so we can change for benchmarking; these seem to be fairly
 * reasonable for Intel CPUs made since 2010. Please adjust as necessary if
 * or when your CPU has more load / execute units. We've written benchmark code
 * to help you tune your platform, see crc64Test. */
#if defined(__i386__) || defined(__X86_64__)
static size_t CRC64_TRI_CUTOFF = (2*1024);
static size_t CRC64_DUAL_CUTOFF = (128);
#else
static size_t CRC64_TRI_CUTOFF = (16*1024);
static size_t CRC64_DUAL_CUTOFF = (1024);
#endif


void set_crc64_cutoffs(size_t dual_cutoff, size_t tri_cutoff) {
    CRC64_DUAL_CUTOFF = dual_cutoff;
    CRC64_TRI_CUTOFF = tri_cutoff;
}

/* Calculate a non-inverted CRC multiple bytes at a time on a little-endian
 * architecture. If you need inverted CRC, invert *before* calling and invert
 * *after* calling.
 * 64 bit crc = process 8/16/24 bytes at once;
 */
uint64_t crcspeed64little(uint64_t little_table[8][256], uint64_t crc1,
                          void *buf, size_t len) {
    unsigned char *next1 = buf;

    if (CRC64_DUAL_CUTOFF < 1) {
        goto final;
    }

    /* process individual bytes until we reach an 8-byte aligned pointer */
    while (len && ((uintptr_t)next1 & 7) != 0) {
        crc1 = little_table[0][(crc1 ^ *next1++) & 0xff] ^ (crc1 >> 8);
        len--;
    }

    if (len >  CRC64_TRI_CUTOFF) {
        /* 24 bytes per loop, doing 3 parallel 8 byte chunks at a time */
        unsigned char *next2, *next3;
        uint64_t olen, crc2=0, crc3=0;
        CRC64_SPLIT(3);
        /* len is now the length of the first segment, the 3rd segment possibly
         * having extra bytes to clean up at the end
         */
        next3 = next2 + len;
        while (len >= 8) {
            len -= 8;
            DO_8_1(crc1, next1);
            DO_8_1(crc2, next2);
            DO_8_1(crc3, next3);
            DO_8_2(crc1);
            DO_8_2(crc2);
            DO_8_2(crc3);
        }

        /* merge the 3 crcs */
        MERGE_CRC(crc2);
        MERGE_CRC(crc3);
        MERGE_END(next3, 3);
    } else if (len > CRC64_DUAL_CUTOFF) {
        /* 16 bytes per loop, doing 2 parallel 8 byte chunks at a time */
        unsigned char *next2;
        uint64_t olen, crc2=0;
        CRC64_SPLIT(2);
        /* len is now the length of the first segment, the 2nd segment possibly
         * having extra bytes to clean up at the end
         */
        while (len >= 8) {
            len -= 8;
            DO_8_1(crc1, next1);
            DO_8_1(crc2, next2);
            DO_8_2(crc1);
            DO_8_2(crc2);
        }

        /* merge the 2 crcs */
        MERGE_CRC(crc2);
        MERGE_END(next2, 2);
    }
    /* We fall through here to handle our <CRC64_DUAL_CUTOFF inputs, and for any trailing
     * bytes that wasn't evenly divisble by 16 or 24 above. */

    /* fast processing, 8 bytes (aligned!) per loop */
    while (len >= 8) {
        len -= 8;
        DO_8_1(crc1, next1);
        DO_8_2(crc1);
    }
final:
    /* process remaining bytes (can't be larger than 8) */
    while (len) {
        crc1 = little_table[0][(crc1 ^ *next1++) & 0xff] ^ (crc1 >> 8);
        len--;
    }

    return crc1;
}

/* clean up our namespace */
#undef DO_8_1
#undef DO_8_2
#undef CRC64_SPLIT
#undef MERGE_CRC
#undef MERGE_END
#undef CRC64_REVERSED_POLY
#undef CRC64_LEN_MASK



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

    /* note: alignment + 2/3-way processing can probably be handled here nearly
       the same as above, using our updated DO_8_2 macro. Not included in these
       changes, as other authors, I don't have big-endian to test with. */

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

/* Initialize CRC lookup table in architecture-dependent manner. */
void crcspeed64native_init(crcfn64 fn, uint64_t table[8][256]) {
    uint64_t n = 1;

    *(char *)&n ? crcspeed64little_init(fn, table)
                : crcspeed64big_init(fn, table);
}


/* Return the CRC of buf[0..len-1] with initial crc, processing eight bytes
   at a time using passed-in lookup table.
   This selects one of two routines depending on the endianness of
   the architecture. */
uint64_t crcspeed64native(uint64_t table[8][256], uint64_t crc, void *buf,
                          size_t len) {
    uint64_t n = 1;

    return *(char *)&n ? crcspeed64little(table, crc, buf, len)
                       : crcspeed64big(table, crc, buf, len);
}