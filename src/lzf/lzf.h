//
// Created by dong on 23-5-22.
//

#ifndef LATTE_C_LZF_H
#define LATTE_C_LZF_H
/***********************************************************************
**
**	lzf -- an extremely fast/free compression/decompression-method
**	http://liblzf.plan9.de/
**
**	This algorithm is believed to be patent-free.
**
***********************************************************************/
#include <stdio.h>
#define LZF_VERSION 0x0105 /* 1.5, API version */

/*
 * Compress in_len bytes stored at the memory block starting at
 * in_data and write the result to out_data, up to a maximum length
 * of out_len bytes.
 *
 * If the output buffer is not large enough or any error occurs return 0,
 * otherwise return the number of bytes used, which might be considerably
 * more than in_len (but less than 104% of the original size), so it
 * makes sense to always use out_len == in_len - 1), to ensure _some_
 * compression, and store the data uncompressed otherwise (with a flag, of
 * course.
 *
 * lzf_compress might use different algorithms on different systems and
 * even different runs, thus might result in different compressed strings
 * depending on the phase of the moon or similar factors. However, all
 * these strings are architecture-independent and will result in the
 * original data when decompressed using lzf_decompress.
 *
 * The buffers must not be overlapping.
 *
 * If the option LZF_STATE_ARG is enabled, an extra argument must be
 * supplied which is not reflected in this header file. Refer to lzfP.h
 * and lzf_c.c.
 *
 */
size_t
lzf_compress (const void *const in_data,  size_t in_len,
              void             *out_data, size_t out_len);

/*
 * Decompress data compressed with some version of the lzf_compress
 * function and stored at location in_data and length in_len. The result
 * will be stored at out_data up to a maximum of out_len characters.
 *
 * If the output buffer is not large enough to hold the decompressed
 * data, a 0 is returned and errno is set to E2BIG. Otherwise the number
 * of decompressed bytes (i.e. the original length of the data) is
 * returned.
 *
 * If an error in the compressed data is detected, a zero is returned and
 * errno is set to EINVAL.
 *
 * This function is very fast, about as fast as a copying loop.
 */
size_t
lzf_decompress (const void *const in_data,  size_t in_len,
                void             *out_data, size_t out_len);

size_t hello_c();
size_t hello_d();
#endif //LATTE_C_LZF_H
