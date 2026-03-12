/**
 * @file solarisfixes.h
 * @brief Solaris 平台兼容性修复
 *        在 Solaris + GCC 环境下，修复 <math.h> 中 isnan/isfinite/isinf 宏
 *        与 GCC 内置函数不兼容的问题，同时补充缺失的 u_int/u_int32_t 类型别名。
 *        仅在 __sun 平台生效，其他平台此文件为空操作。
 *
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 */
/* Solaris specific fixes.
 *
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

#if defined(__sun)

#if defined(__GNUC__)
#include <math.h>

/**
 * @brief Solaris/GCC 修复：使用 GCC 内置函数重定义 isnan
 *        Solaris 的 isnan 宏与 GCC 内置不兼容，用 __builtin_expect 优化版本替换。
 * @param x 待检测的浮点值
 */
#undef isnan
#define isnan(x) \
     __extension__({ __typeof (x) __x_a = (x); \
     __builtin_expect(__x_a != __x_a, 0); })

/**
 * @brief Solaris/GCC 修复：使用 GCC 内置函数重定义 isfinite
 * @param x 待检测的浮点值
 */
#undef isfinite
#define isfinite(x) \
     __extension__ ({ __typeof (x) __x_f = (x); \
     __builtin_expect(!isnan(__x_f - __x_f), 1); })

/**
 * @brief Solaris/GCC 修复：使用 GCC 内置函数重定义 isinf
 * @param x 待检测的浮点值
 */
#undef isinf
#define isinf(x) \
     __extension__ ({ __typeof (x) __x_i = (x); \
     __builtin_expect(!isnan(__x_i) && !isfinite(__x_i), 0); })

/** @brief Solaris 缺失的 u_int 类型别名 */
#define u_int uint
/** @brief Solaris 缺失的 u_int32_t 类型别名 */
#define u_int32_t uint32_t
#endif /* __GNUC__ */

#endif /* __sun */
