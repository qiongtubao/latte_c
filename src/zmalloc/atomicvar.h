/**
 * @file atomicvar.h
 * @brief zmalloc 模块的原子操作接口（zmalloc_used_memory 专用）
 *        基于 C11 _Atomic、GCC __atomic 或 GCC __sync 宏实现，
 *        用于线程安全地更新 zmalloc 已分配内存计数器。
 *        优先级：C11 _Atomic > GCC __atomic > GCC __sync，
 *        均不支持时编译报错。
 *
 * Copyright (c) 2015, Salvatore Sanfilippo <antirez at gmail dot com>
 */
/* This file implements atomic counters using c11 _Atomic, __atomic or __sync
 * macros if available, otherwise we will throw an error when compile.
 *
 * The exported interface is composed of three macros:
 *
 * latte_atomic_incr(var,count) -- Increment the atomic counter
 * latte_atomic_get_incr(var,oldvalue_var,count) -- Get and increment the atomic counter
 * latte_atomic_decr(var,count) -- Decrement the atomic counter
 * latte_atomic_get(var,dstvar) -- Fetch the atomic counter value
 * latte_atomic_set(var,value)  -- Set the atomic counter value
 * latte_atomic_get_with_sync(var,value)  -- 'latte_atomic_get' with inter-thread synchronization
 * latte_atomic_set_with_sync(var,value)  -- 'latte_atomic_set' with inter-thread synchronization
 *
 * Never use return value from the macros, instead use the AtomicGetIncr()
 * if you need to get the current value and increment it atomically, like
 * in the following example:
 *
 *  long oldvalue;
 *  latte_atomic_get_incr(myvar,oldvalue,1);
 *  doSomethingWith(oldvalue);
 *
 * ----------------------------------------------------------------------------
 *
 * Copyright (c) 2015, Salvatore Sanfilippo <antirez at gmail dot com>
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

#include <pthread.h>


#ifndef __ATOMIC_VAR_H
#define __ATOMIC_VAR_H

/**
 * @brief 原子变量修饰符宏（默认为空，C11 模式下展开为 _Atomic）
 *        使用方式：latteAtomic size_t zmalloc_used_memory;
 */
#define latteAtomic

/* 若定义 __ATOMIC_VAR_FORCE_SYNC_MACROS，强制使用 __sync 宏以便 Helgrind 检测。
 * Helgrind 能识别 __sync 宏的线程间顺序关系，从而减少误报。 */
// #define __ATOMIC_VAR_FORCE_SYNC_MACROS

/* 使用 helgrind.h 宏向 Helgrind 声明 happens-before 关系，避免误报。
 * 详见：valgrind/helgrind.h 和 https://www.valgrind.org/docs/manual/hg-manual.html
 * 这些宏仅在 `make helgrind` 时生效（需预先安装 Valgrind）。*/
#ifdef __ATOMIC_VAR_FORCE_SYNC_MACROS
#include <valgrind/helgrind.h>
#else
/** @brief 标注"在此处之前发生"（非 Helgrind 编译时为空操作） */
#define ANNOTATE_HAPPENS_BEFORE(v) ((void) v)
/** @brief 标注"在此处之后发生"（非 Helgrind 编译时为空操作） */
#define ANNOTATE_HAPPENS_AFTER(v)  ((void) v)
#endif

#if !defined(__ATOMIC_VAR_FORCE_SYNC_MACROS) && defined(__STDC_VERSION__) && \
    (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)
/* ---- C11 _Atomic 实现（最优先，编译器原生支持） ---- */

/** @brief C11 模式下原子变量修饰符为 _Atomic */
#undef  latteAtomic
#define latteAtomic _Atomic

#include <stdatomic.h>
/** @brief 原子加：var += count（relaxed 内存序） */
#define latte_atomic_incr(var,count) atomic_fetch_add_explicit(&var,(count),memory_order_relaxed)
/** @brief 先取再加：oldvalue_var = var，var += count（relaxed 内存序） */
#define latte_atomic_get_incr(var,oldvalue_var,count) do { \
    oldvalue_var = atomic_fetch_add_explicit(&var,(count),memory_order_relaxed); \
} while(0)
/** @brief 原子减：var -= count（relaxed 内存序） */
#define latte_atomic_decr(var,count) atomic_fetch_sub_explicit(&var,(count),memory_order_relaxed)
/** @brief 原子读：dstvar = var（relaxed 内存序） */
#define latte_atomic_get(var,dstvar) do { \
    dstvar = atomic_load_explicit(&var,memory_order_relaxed); \
} while(0)
/** @brief 原子写：var = value（relaxed 内存序） */
#define latte_atomic_set(var,value) atomic_store_explicit(&var,value,memory_order_relaxed)
/** @brief 带顺序一致性屏障的原子读：dstvar = var（seq_cst 内存序） */
#define latte_atomic_get_with_sync(var,dstvar) do { \
    dstvar = atomic_load_explicit(&var,memory_order_seq_cst); \
} while(0)
/** @brief 带顺序一致性屏障的原子写：var = value（seq_cst 内存序） */
#define latte_atomic_set_with_sync(var,value) \
    atomic_store_explicit(&var,value,memory_order_seq_cst)
/** @brief 当前使用的原子操作 API 名称 */
#define REDIS_ATOMIC_API "c11-builtin"

#elif !defined(__ATOMIC_VAR_FORCE_SYNC_MACROS) && \
    (!defined(__clang__) || !defined(__APPLE__) || __apple_build_version__ > 4210057) && \
    defined(__ATOMIC_RELAXED) && defined(__ATOMIC_SEQ_CST)
/* ---- GCC __atomic 内置宏实现 ---- */

/** @brief 原子加：var += count */
#define latte_atomic_incr(var,count) __atomic_add_fetch(&var,(count),__ATOMIC_RELAXED)
/** @brief 先取再加：oldvalue_var = var，var += count */
#define latte_atomic_get_incr(var,oldvalue_var,count) do { \
    oldvalue_var = __atomic_fetch_add(&var,(count),__ATOMIC_RELAXED); \
} while(0)
/** @brief 原子减：var -= count */
#define latte_atomic_decr(var,count) __atomic_sub_fetch(&var,(count),__ATOMIC_RELAXED)
/** @brief 原子读：dstvar = var */
#define latte_atomic_get(var,dstvar) do { \
    dstvar = __atomic_load_n(&var,__ATOMIC_RELAXED); \
} while(0)
/** @brief 原子写：var = value */
#define latte_atomic_set(var,value) __atomic_store_n(&var,value,__ATOMIC_RELAXED)
/** @brief 带顺序一致性屏障的原子读 */
#define latte_atomic_get_with_sync(var,dstvar) do { \
    dstvar = __atomic_load_n(&var,__ATOMIC_SEQ_CST); \
} while(0)
/** @brief 带顺序一致性屏障的原子写 */
#define latte_atomic_set_with_sync(var,value) \
    __atomic_store_n(&var,value,__ATOMIC_SEQ_CST)
/** @brief 当前使用的原子操作 API 名称 */
#define REDIS_ATOMIC_API "atomic-builtin"

#elif defined(HAVE_ATOMIC)
/* ---- GCC __sync 内置宏实现（兼容 Helgrind） ---- */

/** @brief 原子加：var += count */
#define latte_atomic_incr(var,count) __sync_add_and_fetch(&var,(count))
/** @brief 先取再加：oldvalue_var = var，var += count */
#define latte_atomic_get_incr(var,oldvalue_var,count) do { \
    oldvalue_var = __sync_fetch_and_add(&var,(count)); \
} while(0)
/** @brief 原子减：var -= count */
#define latte_atomic_decr(var,count) __sync_sub_and_fetch(&var,(count))
/** @brief 原子读：dstvar = var */
#define latte_atomic_get(var,dstvar) do { \
    dstvar = __sync_sub_and_fetch(&var,0); \
} while(0)
/** @brief 原子写：var = value（CAS 循环实现） */
#define latte_atomic_set(var,value) do { \
    while(!__sync_bool_compare_and_swap(&var,var,value)); \
} while(0)
/* __sync 内置宏默认发出全内存屏障 */
/** @brief 带全内存屏障的原子读 */
#define latte_atomic_get_with_sync(var,dstvar) { \
    dstvar = __sync_sub_and_fetch(&var,0,__sync_synchronize); \
    ANNOTATE_HAPPENS_AFTER(&var); \
} while(0)
/** @brief 带全内存屏障的原子写 */
#define latte_atomic_set_with_sync(var,value) do { \
    ANNOTATE_HAPPENS_BEFORE(&var);  \
    while(!__sync_bool_compare_and_swap(&var,var,value,__sync_synchronize)); \
} while(0)
/** @brief 当前使用的原子操作 API 名称 */
#define REDIS_ATOMIC_API "sync-builtin"

#else
#error "Unable to determine atomic operations for your platform"

#endif
#endif /* __ATOMIC_VAR_H */
