/**
 * @file lzf.h
 * @brief LZF 极速无损压缩/解压算法接口
 *        LZF 是一种基于字典的流式压缩算法，速度极快，压缩率适中
 *        版本：1.5（API version 0x0105）
 *        算法被认为无专利限制，可自由使用
 */
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

/** @brief LZF API 版本号：1.5 */
#define LZF_VERSION 0x0105 /* 1.5, API version */

/**
 * @brief 压缩数据块
 *        将 in_data 起始的 in_len 字节压缩后写入 out_data，
 *        最多写入 out_len 字节。
 *
 * 使用建议：
 *   - 建议 out_len = in_len - 1，当压缩后结果不小于原始大小时返回 0，
 *     调用方可将原始数据原样存储（加标志位区分压缩/非压缩）。
 *   - 不同平台/运行时可能产生不同的压缩字节序列，但均可被 lzf_decompress 正确还原。
 *   - 输入输出缓冲区不可重叠。
 *
 * @param in_data  输入数据指针
 * @param in_len   输入数据长度（字节）
 * @param out_data 输出缓冲区指针
 * @param out_len  输出缓冲区容量（字节）
 * @return 成功时返回压缩后字节数（> 0）；缓冲区不足或出错返回 0
 */
size_t
lzf_compress (const void *const in_data,  size_t in_len,
              void             *out_data, size_t out_len);

/**
 * @brief 解压由 lzf_compress 压缩的数据块
 *        将 in_data 起始的 in_len 字节解压后写入 out_data，
 *        最多写入 out_len 字节。
 *
 * 错误情况：
 *   - 输出缓冲区不足：返回 0，errno 设为 E2BIG
 *   - 压缩数据损坏：返回 0，errno 设为 EINVAL
 *
 * @param in_data  压缩数据指针
 * @param in_len   压缩数据长度（字节）
 * @param out_data 解压输出缓冲区指针
 * @param out_len  输出缓冲区容量（字节）
 * @return 成功时返回解压后原始字节数；失败返回 0
 */
size_t
lzf_decompress (const void *const in_data,  size_t in_len,
                void             *out_data, size_t out_len);

/**
 * @brief 压缩自测函数（调试用）
 * @return 压缩后字节数
 */
size_t hello_c();

/**
 * @brief 解压自测函数（调试用）
 * @return 解压后字节数
 */
size_t hello_d();

#endif //LATTE_C_LZF_H
