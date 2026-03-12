/**
 * @file crc64.h
 * @brief 高速 CRC-64 计算接口
 *        基于查表法（8路并行，table[8][256]）实现小端/大端/本机字节序的 CRC-64 计算。
 *        支持为不同数据规模选择单路/双路/三路并行截止阈值，以平衡延迟与吞吐量。
 *
 * Copyright (c) 2014, Matt Stancliff <matt@genges.com>
 */

/* Copyright (c) 2014, Matt Stancliff <matt@genges.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. */

#ifndef CRC64_H
#define CRC64_H

#include <inttypes.h>
#include <stdio.h>

/**
 * @brief CRC-64 基础计算函数指针类型
 *        接受当前 CRC 值、数据缓冲区和长度，返回更新后的 CRC-64 值。
 * @param crc  当前 CRC-64 累积值
 * @param buf  输入数据缓冲区
 * @param len  数据长度（字节）
 * @return 更新后的 CRC-64 值
 */
typedef uint64_t (*crcfn64)(uint64_t, const void *, const uint64_t);

/**
 * @brief 设置 CRC-64 并行计算的数据规模截止阈值
 *        当数据长度小于 dual_cutoff 时使用单路；
 *        小于 tri_cutoff 时使用双路；大于等于 tri_cutoff 时使用三路并行。
 * @param dual_cutoff 切换到双路并行的最小数据长度（字节）
 * @param tri_cutoff  切换到三路并行的最小数据长度（字节）
 */
void set_crc64_cutoffs(size_t dual_cutoff, size_t tri_cutoff);

/**
 * @brief 以小端字节序初始化 CRC-64 查找表
 *        使用给定的基础 CRC 函数 fn 生成 8 路并行查找表。
 * @param fn    基础 CRC-64 计算函数
 * @param table 输出：8x256 查找表（调用方负责分配）
 */
void crcspeed64little_init(crcfn64 fn, uint64_t table[8][256]);

/**
 * @brief 以大端字节序初始化 CRC-64 查找表
 * @param fn    基础 CRC-64 计算函数
 * @param table 输出：8x256 查找表（调用方负责分配）
 */
void crcspeed64big_init(crcfn64 fn, uint64_t table[8][256]);

/**
 * @brief 以本机字节序初始化 CRC-64 查找表
 *        自动检测系统字节序后调用对应的 little/big 初始化函数。
 * @param fn    基础 CRC-64 计算函数
 * @param table 输出：8x256 查找表（调用方负责分配）
 */
void crcspeed64native_init(crcfn64 fn, uint64_t table[8][256]);

/**
 * @brief 使用小端字节序查找表计算数据的 CRC-64（高速）
 * @param table 由 crcspeed64little_init 初始化的查找表
 * @param crc   初始 CRC-64 值（首次调用传 0）
 * @param buf   输入数据缓冲区
 * @param len   数据长度（字节）
 * @return 计算后的 CRC-64 值
 */
uint64_t crcspeed64little(uint64_t table[8][256], uint64_t crc, void *buf,
                          size_t len);

/**
 * @brief 使用大端字节序查找表计算数据的 CRC-64（高速）
 * @param table 由 crcspeed64big_init 初始化的查找表
 * @param crc   初始 CRC-64 值（首次调用传 0）
 * @param buf   输入数据缓冲区
 * @param len   数据长度（字节）
 * @return 计算后的 CRC-64 值
 */
uint64_t crcspeed64big(uint64_t table[8][256], uint64_t crc, void *buf,
                       size_t len);

/**
 * @brief 使用本机字节序查找表计算数据的 CRC-64（高速）
 * @param table 由 crcspeed64native_init 初始化的查找表
 * @param crc   初始 CRC-64 值（首次调用传 0）
 * @param buf   输入数据缓冲区
 * @param len   数据长度（字节）
 * @return 计算后的 CRC-64 值
 */
uint64_t crcspeed64native(uint64_t table[8][256], uint64_t crc, void *buf,
                        size_t len);


#endif
