





/**
 * @file crc64.c
 * @brief 高性能64位CRC校验算法实现
 *        提供快速的64位循环冗余校验功能，支持多种优化策略
 *        包含CRC合并、查表加速、向量化计算等高级特性
 *        支持大端和小端字节序，可配置双/三路并行处理
 */

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

/**
 * @brief 使用开关语句优化的GF2矩阵向量乘法
 * @param mat 矩阵指针，包含预计算的矩阵行数据
 * @param vec 向量值，用于与矩阵相乘
 * @return 矩阵向量乘法的结果
 */
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
		/* 反转case顺序在Xeon E5-2670上会慢约10% */
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
		vec >>= 4;  // 向右移动4位，处理下一个4位块
		mat += 4;   // 矩阵指针移动到下一组4行
	}
	return sum;
}

#define CRC_MULTIPLY gf2_matrix_times_switch

#else

/*
	Warning: here there be dragons involving vector math, and macros to save us
	from repeating the same information over and over.
*/

/**
 * @brief 使用向量优化的GF2矩阵向量乘法（双路处理）
 * @param mat 矩阵指针，包含预计算的矩阵行数据
 * @param vec 向量值，用于与矩阵相乘
 * @return 矩阵向量乘法的结果
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
	/* 这个表允许我们在gf2_matrix_times_vec2()中消除条件判断 */
	static v2uq masks2[4] = {
		{0,0},
		{-1,0},
		{0,-1},
		{-1,-1},
	};

	/* 与gf2_matrix_times_vec几乎一样优美，但每步只处理一半的位数，
	 * 所以我们需要每个chunk4操作2次。在我的测试中更快。 */

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

/**
 * @brief 计算GF2矩阵的平方
 * @param square 输出矩阵，存储计算结果
 * @param mat 输入矩阵
 * @param dim 矩阵维度
 */
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

/**
 * @brief 初始化CRC合并缓存表
 * @param poly CRC多项式
 * @param dim 矩阵维度
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

	/* do/while循环用于覆盖前两层，它们不被使用，但在最后两层中为Redis多项式重新生成 */
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
/**
 * @brief 合并两个连续块的CRC-64值
 * @param crc1 第一个块的CRC-64值
 * @param crc2 第二个块的CRC-64值
 * @param len2 第二个块的长度
 * @param poly CRC多项式
 * @param dim 矩阵维度
 * @return 合并后的CRC-64值
 */
uint64_t crc64_combine(uint64_t crc1, uint64_t crc2, uintmax_t len2, uint64_t poly, uint8_t dim) {
	/* 退化情况：第二个块长度为0 */
	if (len2 == 0)
		return crc1;

	unsigned cache_num = 0;
	if (combine_cache[0][0] == 0) {
		init_combine_cache(poly, dim);
	}

	/* 对crc1应用len2个零字节（第一次平方将在even中放置一个零字节、八个零位的操作符） */
	do
	{
		/* 对len2的当前位应用零操作符 */
		if (len2 & 1)
			crc1 = CRC_MULTIPLY(combine_cache[cache_num], crc1);
		len2 >>= 1;
		cache_num = (cache_num + 1) & 63;
		/* 如果没有更多位设置，则完成 */
	} while (len2 != 0);

	/* 返回合并后的crc */
	crc1 ^= crc2;
	return crc1;
}



#define CRC64_LEN_MASK UINT64_C(0x7ffffffffffffff8)
#define CRC64_REVERSED_POLY UINT64_C(0x95ac9329ac4bc9b5)

/* 填充CRC常数表 */
/**
 * @brief 为小端序架构初始化CRC64快速查找表
 * @param crcfn CRC计算函数指针
 * @param table 8x256的CRC查找表
 */
void crcspeed64little_init(crcfn64 crcfn, uint64_t table[8][256]) {
    uint64_t crc;

    /* 为所有单字节序列生成CRC */
    for (int n = 0; n < 256; n++) {
        unsigned char v = n;
        table[0][n] = crcfn(0, &v, 1);
    }

    /* 为未来的slice-by-8/16/24+查找生成嵌套CRC表 */
    for (int n = 0; n < 256; n++) {
        crc = table[0][n];
        for (int k = 1; k < 8; k++) {
            crc = table[0][crc & 0xff] ^ (crc >> 8);
            table[k][n] = crc;
        }
    }
#if USE_STATIC_COMBINE_CACHE
    /* 为slice-by-16/24+的CRC拼接初始化合并缓存 */
    init_combine_cache(CRC64_REVERSED_POLY, 64);
#endif
}


/* 反转64位字中的字节顺序 */
/**
 * @brief 反转64位字中的字节顺序
 * @param a 要反转的64位值
 * @return 字节序反转后的64位值
 */
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

/* 此函数被调用一次，用于初始化在大端序架构上使用的CRC表 */
/**
 * @brief 为大端序架构初始化CRC64快速查找表
 * @param fn CRC计算函数指针
 * @param big_table 8x256的大端序CRC查找表
 */
void crcspeed64big_init(crcfn64 fn, uint64_t big_table[8][256]) {
    /* 创建小端序表然后反转所有条目 */
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


/**
 * @brief 设置CRC64并行处理的阈值
 * @param dual_cutoff 双路处理的最小字节数阈值
 * @param tri_cutoff 三路处理的最小字节数阈值
 */
void set_crc64_cutoffs(size_t dual_cutoff, size_t tri_cutoff) {
    CRC64_DUAL_CUTOFF = dual_cutoff;
    CRC64_TRI_CUTOFF = tri_cutoff;
}

/**
 * @brief 在小端序架构上一次处理多个字节计算非反转CRC
 * @param little_table 预计算的CRC查找表
 * @param crc1 初始CRC值
 * @param buf 输入数据缓冲区
 * @param len 数据长度
 * @return 计算得到的CRC64值
 */
uint64_t crcspeed64little(uint64_t little_table[8][256], uint64_t crc1,
                          void *buf, size_t len) {
    unsigned char *next1 = buf;

    if (CRC64_DUAL_CUTOFF < 1) {
        goto final;
    }

    /* 处理单个字节直到我们达到8字节对齐指针 */
    while (len && ((uintptr_t)next1 & 7) != 0) {
        crc1 = little_table[0][(crc1 ^ *next1++) & 0xff] ^ (crc1 >> 8);
        len--;
    }

    if (len >  CRC64_TRI_CUTOFF) {
        /* 每循环24字节，同时处理3个并行的8字节块 */
        unsigned char *next2, *next3;
        uint64_t olen, crc2=0, crc3=0;
        CRC64_SPLIT(3);
        /* len现在是第一段的长度，第3段可能有额外的字节需要在最后清理 */
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

        /* 合并3个crc值 */
        MERGE_CRC(crc2);
        MERGE_CRC(crc3);
        MERGE_END(next3, 3);
    } else if (len > CRC64_DUAL_CUTOFF) {
        /* 每循环16字节，同时处理2个并行的8字节块 */
        unsigned char *next2;
        uint64_t olen, crc2=0;
        CRC64_SPLIT(2);
        /* len现在是第一段的长度，第2段可能有额外的字节需要在最后清理 */
        while (len >= 8) {
            len -= 8;
            DO_8_1(crc1, next1);
            DO_8_1(crc2, next2);
            DO_8_2(crc1);
            DO_8_2(crc2);
        }

        /* 合并2个crc值 */
        MERGE_CRC(crc2);
        MERGE_END(next2, 2);
    }
    /* 我们在这里处理<CRC64_DUAL_CUTOFF的输入，以及上面不能被16或24整除的尾随字节 */

    /* 快速处理，每次循环8字节（对齐！） */
    while (len >= 8) {
        len -= 8;
        DO_8_1(crc1, next1);
        DO_8_2(crc1);
    }
final:
    /* 处理剩余字节（不能大于8） */
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



/**
 * @brief 在大端序架构上一次计算8字节的非反转CRC
 * @param big_table 预计算的大端序CRC查找表
 * @param crc 初始CRC值
 * @param buf 输入数据缓冲区
 * @param len 数据长度
 * @return 计算得到的CRC64值
 */
uint64_t crcspeed64big(uint64_t big_table[8][256], uint64_t crc, void *buf,
                       size_t len) {
    unsigned char *next = buf;

    crc = rev8(crc);
    while (len && ((uintptr_t)next & 7) != 0) {
        crc = big_table[0][(crc >> 56) ^ *next++] ^ (crc << 8);
        len--;
    }

    /* 注意：对齐 + 2/3路处理在这里可以用我们更新的DO_8_2宏以几乎相同的方式处理。
       由于没有大端序系统可供测试，所以没有包含在这些更改中。 */

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

/* 以架构相关的方式初始化CRC查找表 */
/**
 * @brief 根据架构字节序初始化CRC查找表
 * @param fn CRC计算函数指针
 * @param table 8x256的CRC查找表
 */
void crcspeed64native_init(crcfn64 fn, uint64_t table[8][256]) {
    uint64_t n = 1;

    *(char *)&n ? crcspeed64little_init(fn, table)
                : crcspeed64big_init(fn, table);
}


/**
 * @brief 根据架构字节序计算buf[0..len-1]的CRC值
 * @param table 预计算的CRC查找表
 * @param crc 初始CRC值
 * @param buf 输入数据缓冲区
 * @param len 数据长度
 * @return 计算得到的CRC64值
 */
uint64_t crcspeed64native(uint64_t table[8][256], uint64_t crc, void *buf,
                          size_t len) {
    uint64_t n = 1;

    return *(char *)&n ? crcspeed64little(table, crc, buf, len)
                       : crcspeed64big(table, crc, buf, len);
}