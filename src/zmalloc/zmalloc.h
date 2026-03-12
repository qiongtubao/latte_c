/* zmalloc - total amount of allocated memory aware version of malloc()
 *
 * Copyright (c) 2009-2010, Salvatore Sanfilippo <antirez at gmail dot com>
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

/**
 * @file zmalloc.h
 * @brief 带内存统计的 malloc 封装模块
 *        支持 tcmalloc / jemalloc / 系统 libc 三种分配器后端。
 *        所有分配均通过原子计数器 used_memory 跟踪总用量，
 *        OOM 时默认调用 abort()，可通过 zmalloc_set_oom_handler 替换。
 */

#ifndef __ZMALLOC_H
#define __ZMALLOC_H

#include <stddef.h>
/* Double expansion needed for stringification of macro values. */
#define __xstr(s) __str(s)
#define __str(s) #s

#if defined(USE_TCMALLOC)
#define ZMALLOC_LIB ("tcmalloc-" __xstr(TC_VERSION_MAJOR) "." __xstr(TC_VERSION_MINOR))
#include <google/tcmalloc.h>
#if (TC_VERSION_MAJOR == 1 && TC_VERSION_MINOR >= 6) || (TC_VERSION_MAJOR > 1)
#define HAVE_MALLOC_SIZE 1
#define zmalloc_size(p) tc_malloc_size(p)
#else
#error "Newer version of tcmalloc required"
#endif

#elif defined(USE_JEMALLOC)
#define ZMALLOC_LIB ("jemalloc-" __xstr(JEMALLOC_VERSION_MAJOR) "." __xstr(JEMALLOC_VERSION_MINOR) "." __xstr(JEMALLOC_VERSION_BUGFIX))
#include <jemalloc/jemalloc.h>
#if (JEMALLOC_VERSION_MAJOR == 2 && JEMALLOC_VERSION_MINOR >= 1) || (JEMALLOC_VERSION_MAJOR > 2)
#define HAVE_MALLOC_SIZE 1
#define zmalloc_size(p) je_malloc_usable_size(p)
#else
#error "Newer version of jemalloc required"
#endif

#elif defined(__APPLE__)
#include <malloc/malloc.h>
#define HAVE_MALLOC_SIZE 1
#define zmalloc_size(p) malloc_size(p)
#endif

/* On native libc implementations, we should still do our best to provide a
 * HAVE_MALLOC_SIZE capability. This can be set explicitly as well:
 *
 * NO_MALLOC_USABLE_SIZE disables it on all platforms, even if they are
 *      known to support it.
 * USE_MALLOC_USABLE_SIZE forces use of malloc_usable_size() regardless
 *      of platform.
 */
#ifndef ZMALLOC_LIB
#define ZMALLOC_LIB "libc"

#if !defined(NO_MALLOC_USABLE_SIZE) && \
    (defined(__GLIBC__) || defined(__FreeBSD__) || \
     defined(USE_MALLOC_USABLE_SIZE))

/* Includes for malloc_usable_size() */
#ifdef __FreeBSD__
#include <malloc_np.h>
#else
#include <malloc.h>
#endif

#define HAVE_MALLOC_SIZE 1
#define zmalloc_size(p) malloc_usable_size(p)

#endif
#endif

/* We can enable the Redis defrag capabilities only if we are using Jemalloc
 * and the version used is our special version modified for Redis having
 * the ability to return per-allocation fragmentation hints. */
#if defined(USE_JEMALLOC) && defined(JEMALLOC_FRAG_HINT)
#define HAVE_DEFRAG
#endif

/**
 * @brief 分配 size 字节内存，失败时调用 OOM 处理器（默认 abort）
 * @param size 分配字节数
 * @return 非 NULL 内存指针
 */
void *zmalloc(size_t size);

/**
 * @brief 分配 size 字节并清零，失败时调用 OOM 处理器
 * @param size 分配字节数
 * @return 非 NULL 清零内存指针
 */
void *zcalloc(size_t size);

/**
 * @brief 重新分配内存，失败时调用 OOM 处理器
 * @param ptr  原指针（NULL 时等同于 zmalloc）
 * @param size 新大小
 * @return 非 NULL 内存指针
 */
void *zrealloc(void *ptr, size_t size);

/**
 * @brief 尝试分配 size 字节，失败返回 NULL（不触发 OOM 处理器）
 * @param size 分配字节数
 * @return 成功返回指针，失败返回 NULL
 */
void *ztrymalloc(size_t size);

/**
 * @brief 尝试分配并清零，失败返回 NULL
 * @param size 分配字节数
 * @return 成功返回清零指针，失败返回 NULL
 */
void *ztrycalloc(size_t size);

/**
 * @brief 尝试重新分配，失败返回 NULL
 * @param ptr  原指针
 * @param size 新大小
 * @return 成功返回指针，失败返回 NULL
 */
void *ztryrealloc(void *ptr, size_t size);

/**
 * @brief 释放内存并更新内存统计计数
 * @param ptr 要释放的指针（NULL 时安全跳过）
 */
void zfree(void *ptr);

/**
 * @brief 分配内存并通过 usable 返回实际可用大小，失败触发 OOM
 * @param size   请求字节数
 * @param usable 输出实际可用大小（非 NULL 时填充）
 * @return 非 NULL 内存指针
 */
void *zmalloc_usable(size_t size, size_t *usable);

/**
 * @brief 分配并清零，通过 usable 返回实际可用大小，失败触发 OOM
 * @param size   请求字节数
 * @param usable 输出实际可用大小
 * @return 非 NULL 清零内存指针
 */
void *zcalloc_usable(size_t size, size_t *usable);

/**
 * @brief 重新分配并通过 usable 返回实际可用大小，失败触发 OOM
 * @param ptr    原指针
 * @param size   新大小
 * @param usable 输出实际可用大小
 * @return 非 NULL 内存指针
 */
void *zrealloc_usable(void *ptr, size_t size, size_t *usable);

/**
 * @brief 尝试分配并返回实际可用大小，失败返回 NULL
 * @param size   请求字节数
 * @param usable 输出实际可用大小
 * @return 成功返回指针，失败返回 NULL
 */
void *ztrymalloc_usable(size_t size, size_t *usable);

/**
 * @brief 尝试分配并清零，返回实际可用大小，失败返回 NULL
 * @param size   请求字节数
 * @param usable 输出实际可用大小
 * @return 成功返回指针，失败返回 NULL
 */
void *ztrycalloc_usable(size_t size, size_t *usable);

/**
 * @brief 尝试重新分配并返回实际可用大小，失败返回 NULL
 * @param ptr    原指针
 * @param size   新大小
 * @param usable 输出实际可用大小
 * @return 成功返回指针，失败返回 NULL
 */
void *ztryrealloc_usable(void *ptr, size_t size, size_t *usable);

/**
 * @brief 释放内存并输出被释放的字节数
 * @param ptr    要释放的指针
 * @param usable 输出实际释放的字节数
 */
void zfree_usable(void *ptr, size_t *usable);

/**
 * @brief 复制字符串（zmalloc 版 strdup）
 * @param s 原始字符串
 * @return 新分配的副本指针
 */
char *zstrdup(const char *s);

/**
 * @brief 获取当前已分配的总内存字节数（原子读取）
 * @return 已用内存字节数
 */
size_t zmalloc_used_memory(void);

/**
 * @brief 设置自定义 OOM 处理器（替换默认的 abort()）
 * @param oom_handler 新的处理函数，参数为请求分配的字节数
 */
void zmalloc_set_oom_handler(void (*oom_handler)(size_t));

/**
 * @brief 获取进程当前的 RSS（物理内存占用）字节数
 *        实现依赖平台（/proc/stat、taskinfo、sysctl 等）
 * @return RSS 字节数；不支持时返回 zmalloc_used_memory()
 */
size_t zmalloc_get_rss(void);

/**
 * @brief 获取 jemalloc 分配器统计信息（allocated/active/resident）
 * @param allocated 输出已分配字节数
 * @param active    输出活跃字节数
 * @param resident  输出常驻字节数
 * @return 始终返回 1
 */
int zmalloc_get_allocator_info(size_t *allocated, size_t *active, size_t *resident);

/**
 * @brief 启用/禁用 jemalloc 后台线程（用于异步 purge）
 * @param enable 非 0 启用，0 禁用
 */
void set_jemalloc_bg_thread(int enable);

/**
 * @brief 触发 jemalloc 将保留页归还给操作系统
 * @return 成功返回 0，失败返回 -1
 */
int jemalloc_purge();

/**
 * @brief 获取指定进程的 Private_Dirty 内存字节数（来自 /proc/<pid>/smaps）
 * @param pid 进程 ID，-1 表示当前进程
 * @return Private_Dirty 字节数
 */
size_t zmalloc_get_private_dirty(long pid);

/**
 * @brief 从 /proc/<pid>/smaps 中读取指定字段的字节数之和
 * @param field 字段名（含冒号，如 "Rss:"）
 * @param pid   进程 ID，-1 表示当前进程
 * @return 指定字段的字节数之和
 */
size_t zmalloc_get_smap_bytes_by_field(char *field, long pid);

/**
 * @brief 获取系统物理内存总量（字节）
 *        通过 sysconf / sysctl 等平台接口获取
 * @return 物理内存字节数；不支持时返回 0
 */
size_t zmalloc_get_memory_size(void);

/**
 * @brief 调用 libc 原始 free()（绕过 zmalloc 的统计计数）
 *        用于释放 backtrace_symbols() 等系统 API 返回的内存
 * @param ptr 要释放的指针
 */
void zlibc_free(void *ptr);

/**
 * @brief 对指定指针调用 madvise(MADV_DONTNEED) 归还物理页
 *        仅在 jemalloc + Linux 下有效，用于 fork 子进程减少 CoW
 * @param ptr 要归还的内存指针
 */
void zmadvise_dontneed(void *ptr);

void *zrealloc_usable(void *ptr, size_t size, size_t *usable);

#ifdef HAVE_DEFRAG
/**
 * @brief 绕过线程缓存直接释放内存（jemalloc 在线碎片整理专用）
 * @param ptr 要释放的指针
 */
void zfree_no_tcache(void *ptr);

/**
 * @brief 绕过线程缓存直接分配内存（jemalloc 在线碎片整理专用）
 * @param size 分配字节数
 * @return 非 NULL 内存指针
 */
void *zmalloc_no_tcache(size_t size);
#endif

#ifndef HAVE_MALLOC_SIZE
/**
 * @brief 返回 ptr 所在分配块的总大小（含头部 PREFIX_SIZE）
 * @param ptr zmalloc 分配的指针
 * @return 分配块字节数
 */
size_t zmalloc_size(void *ptr);

/**
 * @brief 返回 ptr 可用的有效字节数（不含头部）
 * @param ptr zmalloc 分配的指针
 * @return 可用字节数
 */
size_t zmalloc_usable_size(void *ptr);
#else
#define zmalloc_usable_size(p) zmalloc_size(p)
#endif


#endif /* __ZMALLOC_H */
