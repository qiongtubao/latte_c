/**
 * @file crc.h
 * @brief CRC 校验函数接口
 *        提供 CRC-16/XMODEM、CRC-32C（Castagnoli）、CRC-32/JAMCRC 及 CRC-64/Redis 算法实现。
 */
#ifndef LATTE_CRC_H
#define LATTE_CRC_H
#include <inttypes.h>
#include <stdio.h>

/* ---- CRC-16 ---- */

/**
 * @brief 计算 CRC-16/XMODEM 校验值
 * @param buf 输入数据缓冲区
 * @param len 输入数据字节长度
 * @return 16 位 CRC 校验值
 */
uint16_t crc16xmodem(const char *buf, int len);

/* ---- CRC-32C（Castagnoli） ---- */

/**
 * @brief 计算 CRC-32C 校验值（初始 CRC 为 0）
 * @param buf 输入数据缓冲区
 * @param len 输入数据字节长度
 * @return 32 位 CRC-32C 校验值
 */
uint32_t crc32c(const char* buf, int len);

/**
 * @brief 在已有 CRC 基础上继续计算 CRC-32C（增量计算）
 * @param crc 上次计算的 CRC 值（或初始值 0）
 * @param buf 本次输入数据缓冲区
 * @param len 本次输入数据字节长度
 * @return 更新后的 32 位 CRC-32C 校验值
 */
uint32_t crc32c_extend(uint32_t crc, const char* buf, int len);

/**
 * @brief 对 CRC-32C 值加掩码（用于持久化存储，防止与数据混淆）
 * @param crc 原始 CRC-32C 值
 * @return 加掩码后的 32 位值
 */
uint32_t crc32c_mask(uint32_t crc);

/**
 * @brief 对加掩码的 CRC-32C 值去掩码，还原原始 CRC
 * @param masked_crc 已加掩码的 CRC-32C 值
 * @return 原始 32 位 CRC-32C 值
 */
uint32_t crc32c_unmask(uint32_t masked_crc);

/* ---- CRC-32/JAMCRC ---- */

/**
 * @brief 计算 CRC-32/JAMCRC 校验值
 *        JAMCRC 与标准 CRC-32 的区别：最终不对结果取反（XOR 0xFFFFFFFF）
 * @param buffer 输入数据缓冲区
 * @param len    输入数据字节长度
 * @return 32 位 JAMCRC 校验值
 */
uint32_t crc32jamcrc(const char *buffer, int len);

/* ---- CRC-64（Redis 变体） ---- */

/**
 * @brief 计算 CRC-64（Redis 变体）校验值
 *        使用 Redis 内部定义的 CRC-64 多项式，支持增量计算
 * @param crc 上次计算的 CRC 值（或初始值 0）
 * @param s   输入数据缓冲区
 * @param l   输入数据字节长度
 * @return 更新后的 64 位 CRC 校验值
 */
uint64_t crc64_redis(uint64_t crc, const unsigned char *s, uint64_t l);

#endif
