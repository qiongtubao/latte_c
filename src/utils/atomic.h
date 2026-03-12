/**
 * @file atomic.h
 * @brief 原子操作宏接口
 *        按优先级选择最优原子操作实现：
 *        1. C11 _Atomic（首选，编译器原生支持）
 *        2. GCC __atomic 内置宏
 *        3. GCC __sync 内置宏（兼容 Helgrind 检测）
 *        若均不支持则编译报错。
 *
 * 导出宏接口：
 *   latte_atomic_incr(var, count)            — 原子加 count
 *   latte_atomic_get_incr(var, old, count)   — 先取值再加 count（old 接收旧值）
 *   latte_atomic_decr(var, count)            — 原子减 count
 *   latte_atomic_get(var, dst)               — 原子读取 var 到 dst
 *   latte_atomic_set(var, value)             — 原子写入 value 到 var
 *   latte_atomic_get_with_sync(var, dst)     — 带顺序一致性屏障的原子读取
 *   latte_atomic_set_with_sync(var, value)   — 带顺序一致性屏障的原子写入
 *
 * 注意：不要使用宏的返回值；如需先取值再自增，请使用 latte_atomic_get_incr。
 * 示例：
 *   long oldvalue;
 *   latte_atomic_get_incr(myvar, oldvalue, 1);
 *   doSomethingWith(oldvalue);
 *
 * Copyright (c) 2015, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.（详见文件顶部版权声明）
 */

#include <pthread.h>
#include "sys_config.h"

#ifndef __ATOMIC_VAR_H
#define __ATOMIC_VAR_H

/**
 * @brief 原子变量修饰符宏（默认为空，C11 模式下展开为 _Atomic）
 *        使用方式：latteAtomic size_t counter;
 */
#define latteAtomic

/* 若定义 __ATOMIC_VAR_FORCE_SYNC_MACROS，强制使用 __sync 宏以便 Helgrind 检测 */
// #define __ATOMIC_VAR_FORCE_SYNC_MACROS

/**
 * @brief Helgrind happens-before 标注宏（仅 make helgrind 时生效）
 *        用于告知 Helgrind 线程间的顺序关系，避免误报
 */
#ifdef __ATOMIC_VAR_FORCE_SYNC_MACROS
#include <valgrind/helgrind.h>
#else
/** @brief 标注"在此处之前发生"（Helgrind 专用，非 Helgrind 编译时为空操作） */
#define ANNOTATE_HAPPENS_BEFORE(v) ((void) v)
/** @brief 标注"在此处之后发生"（Helgrind 专用，非 Helgrind 编译时为空操作） */
#define ANNOTATE_HAPPENS_AFTER(v)  ((void) v)
#endif

#if !defined(__ATOMIC_VAR_FORCE_SYNC_MACROS) && defined(__STDC_VERSION__) && \
    (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)
/* ---- C11 _Atomic 实现（最优先） ---- */
#undef  latteAtomic
/** @brief C11 模式下原子变量修饰符为 _Atomic */
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
#define LATTE_ATOMIC_API "sync-builtin"

#else
#error "Unable to determine atomic operations for your platform"

#endif
#endif /* __ATOMIC_VAR_H */
