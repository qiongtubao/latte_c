/**
 * @file fmacros.h
 * @brief 功能测试宏（Feature Test Macros）
 *        在包含任何系统头文件之前定义，用于向 C 标准库声明所需的 POSIX/GNU/XSI 扩展，
 *        以确保跨平台（Linux、macOS、BSD、Solaris、AIX 等）的兼容性。
 *        必须在所有 #include 之前包含此文件。
 *
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 */
/*
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
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
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _REDIS_FMACRO_H
#define _REDIS_FMACRO_H

/** @brief 启用 BSD 来源的扩展 API（所有平台） */
#define _BSD_SOURCE

#if defined(__linux__)
/** @brief Linux：启用 GNU 扩展（glibc 特有 API，如 strndup、qsort_r 等） */
#define _GNU_SOURCE
/** @brief Linux：启用默认来源扩展（替代已弃用的 _BSD_SOURCE） */
#define _DEFAULT_SOURCE
#endif

#if defined(_AIX)
/** @brief AIX：启用所有来源扩展 */
#define _ALL_SOURCE
#endif

#if defined(__linux__) || defined(__OpenBSD__)
/** @brief Linux/OpenBSD：声明 XSI/POSIX.1-2008 扩展（700 = POSIX.1-2008） */
#define _XOPEN_SOURCE 700
/*
 * 在 NetBSD 上，_XOPEN_SOURCE 会取消 _NETBSD_SOURCE 的定义，
 * 从而隐藏 inet_aton 等函数，因此 NetBSD 不定义此宏。
 */
#elif !defined(__NetBSD__)
/** @brief 非 NetBSD 的其他平台：声明 XSI 扩展（不指定版本） */
#define _XOPEN_SOURCE
#endif

#if defined(__sun)
/** @brief Solaris：声明 POSIX.1c 线程扩展 */
#define _POSIX_C_SOURCE 199506L
#endif

/** @brief 启用大文件支持（允许通过 off_t 访问 >2GB 文件） */
#define _LARGEFILE_SOURCE
/** @brief 使 off_t 为 64 位（在 32 位平台上也能访问大文件） */
#define _FILE_OFFSET_BITS 64

#ifdef __linux__
/* features.h 根据上方的宏定义设置具体功能开关 */
#include <features.h>
#endif

#endif
