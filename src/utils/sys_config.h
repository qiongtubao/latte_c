/**
 * @file sys_config.h
 * @brief 系统平台检测与兼容性配置
 *        根据编译目标平台自动检测并定义各类系统特性宏，
 *        为 latte 提供统一的跨平台抽象层。
 *        涵盖：文件系统操作、网络、进程、原子操作、CPU 特性、字节序等。
 *
 * Copyright (c) 2009-Present, Redis Ltd. All rights reserved.
 */
/*
 * Copyright (c) 2009-Present, Redis Ltd.
 * All rights reserved.
 *
 * Licensed under your choice of (a) the Redis Source Available License 2.0
 * (RSALv2); or (b) the Server Side Public License v1 (SSPLv1); or (c) the
 * GNU Affero General Public License v3 (AGPLv3).
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#ifdef __APPLE__
#include <fcntl.h> // for fcntl(fd, F_FULLFSYNC)
#include <AvailabilityMacros.h>
#endif

#ifdef __linux__
#include <features.h>
#include <fcntl.h>
#endif

#if defined(__APPLE__) && defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
#define MAC_OS_10_6_DETECTED
#endif

/**
 * @brief 平台适配的 fstat 宏
 *        macOS 10.6 以下使用 fstat64/stat64，其他平台使用标准 fstat/stat。
 */
#if defined(__APPLE__) && !defined(MAC_OS_10_6_DETECTED)
#define latte_fstat fstat64  /**< macOS 旧版：使用 fstat64 */
#define latte_stat stat64    /**< macOS 旧版：使用 stat64 */
#else
#define latte_fstat fstat    /**< 标准 fstat */
#define latte_stat stat      /**< 标准 stat */
#endif

/** @brief CPU 缓存行大小（Apple ARM64 为 128 字节，其他平台为 64 字节） */
#ifndef CACHE_LINE_SIZE
#if defined(__aarch64__) && defined(__APPLE__)
#define CACHE_LINE_SIZE 128
#else
#define CACHE_LINE_SIZE 64
#endif
#endif

/* ---- /proc 文件系统特性检测（Linux） ---- */
#ifdef __linux__
#define HAVE_PROC_STAT 1           /**< Linux：支持 /proc/self/stat */
#define HAVE_PROC_MAPS 1           /**< Linux：支持 /proc/self/maps */
#define HAVE_PROC_SMAPS 1          /**< Linux：支持 /proc/self/smaps */
#define HAVE_PROC_SOMAXCONN 1      /**< Linux：支持 /proc/sys/net/core/somaxconn */
#define HAVE_PROC_OOM_SCORE_ADJ 1  /**< Linux：支持 /proc/self/oom_score_adj */
#define HAVE_EVENT_FD 1            /**< Linux：支持 eventfd() */
#endif

/* ---- task_info() 支持（macOS） ---- */
#if defined(__APPLE__)
#define HAVE_TASKINFO 1  /**< macOS：支持 task_info() 获取内存使用信息 */
#endif

/* ---- somaxconn 检测 ---- */
#if defined(__APPLE__) || defined(__FreeBSD__)
#define HAVE_SYSCTL_KIPC_SOMAXCONN 1  /**< macOS/FreeBSD：通过 sysctl kipc.somaxconn 获取 */
#elif defined(__OpenBSD__)
#define HAVE_SYSCTL_KERN_SOMAXCONN 1  /**< OpenBSD：通过 sysctl kern.somaxconn 获取 */
#endif

/* ---- backtrace() 支持检测 ---- */
#if defined(__APPLE__) || (defined(__linux__) && defined(__GLIBC__)) || \
    defined(__FreeBSD__) || ((defined(__OpenBSD__) || defined(__NetBSD__) || defined(__sun)) && defined(USE_BACKTRACE))\
 || defined(__DragonFly__) || (defined(__UCLIBC__) && defined(__UCLIBC_HAS_BACKTRACE__))
#define HAVE_BACKTRACE 1  /**< 平台支持 backtrace() 调用栈捕获 */
#endif

/* ---- MSG_NOSIGNAL 支持（Linux） ---- */
#ifdef __linux__
#define HAVE_MSG_NOSIGNAL 1  /**< Linux：send() 支持 MSG_NOSIGNAL 标志（避免 SIGPIPE） */
#if defined(SO_MARK)
#define HAVE_SOCKOPTMARKID 1        /**< Linux：支持 SO_MARK socket 选项 */
#define SOCKOPTMARKID SO_MARK       /**< Linux：socket 标记选项名 */
#endif
#endif

/* ---- 事件轮询 API 检测 ---- */
#ifdef __linux__
#ifndef HAVE_SELECT
#define HAVE_EPOLL 1      /**< Linux：使用 epoll 事件通知 */
#endif
#define HAVE_LIBURING 1   /**< Linux：支持 io_uring（liburing） */
#endif

/* ---- accept4() 支持检测 ---- */
#if defined(__linux__) || defined(OpenBSD5_7) || \
    (__FreeBSD__ >= 10 || __FreeBSD_version >= 1000000) || \
    (defined(NetBSD8_0) || __NetBSD_Version__ >= 800000000)
#define HAVE_ACCEPT4 1  /**< 平台支持 accept4()（含 SOCK_NONBLOCK/SOCK_CLOEXEC 标志） */
#endif

/* ---- kqueue / dispatch 支持（BSD/macOS） ---- */
#if (defined(__APPLE__) && defined(MAC_OS_10_6_DETECTED)) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
#define HAVE_KQUEUE 1    /**< BSD/macOS：使用 kqueue 事件通知 */
/** @brief macOS 默认支持 dispatch（Grand Central Dispatch），如遇异常可在此加平台判断 */
#define HAVE_DISPATCH 1
#endif

/* ---- Solaris evport/psinfo 支持 ---- */
#ifdef __sun
#include <sys/feature_tests.h>
#ifdef _DTRACE_VERSION
#define HAVE_EVPORT 1   /**< Solaris：使用 evport 事件通知 */
#define HAVE_PSINFO 1   /**< Solaris：支持 /proc psinfo 接口 */
#endif
#endif

/* ---- __builtin_prefetch() 支持检测 ---- */
/* Clang 2.9+ / GCC 4.8+ 支持 __builtin_prefetch */
#if defined(__clang__) && (__clang_major__ > 2 || (__clang_major__ == 2 && __clang_minor__ >= 9))
#define HAS_BUILTIN_PREFETCH 1
#elif defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
#define HAS_BUILTIN_PREFETCH 1
#else
#define HAS_BUILTIN_PREFETCH 0
#endif

#if HAS_BUILTIN_PREFETCH
/** @brief 预取内存（读，高时间局部性）：提前加载 addr 到 L1 缓存 */
#define latte_prefetch_read(addr) __builtin_prefetch(addr, 0, 3)
/** @brief 预取内存（写，高时间局部性）：提前标记 addr 将被写入 */
#define latte_prefetch_write(addr) __builtin_prefetch(addr, 1, 3)
#else
#define latte_prefetch_read(addr) ((void)(addr))   /**< 不支持时为空操作 */
#define latte_prefetch_write(addr) ((void)(addr))  /**< 不支持时为空操作 */
#endif

/**
 * @brief 平台适配的文件同步宏
 *        Linux 使用 fdatasync（只同步数据，不同步元数据），
 *        macOS 使用 F_FULLFSYNC（强制写盘），其他平台使用 fsync。
 */
#if defined(__linux__)
#define latte_fsync(fd) fdatasync(fd)
#elif defined(__APPLE__)
#define latte_fsync(fd) fcntl(fd, F_FULLFSYNC)
#else
#define latte_fsync(fd) fsync(fd)
#endif

/* ---- SO_MARK 补充检测（FreeBSD/OpenBSD） ---- */
#if defined(__FreeBSD__)
#if defined(SO_USER_COOKIE)
#define HAVE_SOCKOPTMARKID 1
#define SOCKOPTMARKID SO_USER_COOKIE
#endif
#endif

#if defined(__OpenBSD__)
#if defined(SO_RTABLE)
#define HAVE_SOCKOPTMARKID 1
#define SOCKOPTMARKID SO_RTABLE
#endif
#endif

/**
 * @brief 不可达代码标记宏（同 debug.h 的 latte_unreachable）
 *        GCC 4.5+ / GCC 5+ 使用 __builtin_unreachable，否则 abort。
 */
#if __GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#define latte_unreachable __builtin_unreachable
#else
#define latte_unreachable abort
#endif

/**
 * @brief 分支预测优化宏：标注条件大概率为真
 * @param x 条件表达式
 */
#if __GNUC__ >= 3
#define likely(x) __builtin_expect(!!(x), 1)
/**
 * @brief 分支预测优化宏：标注条件大概率为假
 * @param x 条件表达式
 */
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

/**
 * @brief 禁用 sanitizer 检测的函数属性宏
 *        用于标注已知安全但会触发 sanitizer 误报的函数。
 * @param sanitizer sanitizer 名称字符串（如 "undefined"）
 */
#if defined(__has_attribute)
#if __has_attribute(no_sanitize)
#define LATTE_NO_SANITIZE(sanitizer) __attribute__((no_sanitize(sanitizer)))
#endif
#endif
#if !defined(LATTE_NO_SANITIZE)
#define LATTE_NO_SANITIZE(sanitizer)
#endif

/**
 * @brief 禁用 MemorySanitizer（MSan）的函数属性宏（仅 Clang）
 *        仅 Clang 支持 MSan，GCC 展开为空。
 */
#if defined(__clang__)
#define LATTE_NO_SANITIZE_MSAN(sanitizer) LATTE_NO_SANITIZE(sanitizer)
#else
#define LATTE_NO_SANITIZE_MSAN(sanitizer)
#endif

/**
 * @brief 平台适配的文件范围同步宏
 *        Linux 使用 sync_file_range（细粒度控制），macOS 使用 F_FULLFSYNC，
 *        其他平台退化为 fsync。
 * @param fd   文件描述符
 * @param off  同步起始偏移
 * @param size 同步字节数
 */
#if (defined(__linux__) && defined(SYNC_FILE_RANGE_WAIT_BEFORE))
#define HAVE_SYNC_FILE_RANGE 1
#define rdb_fsync_range(fd,off,size) sync_file_range(fd,off,size,SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE)
#elif defined(__APPLE__)
#define rdb_fsync_range(fd,off,size) fcntl(fd, F_FULLFSYNC)
#else
#define rdb_fsync_range(fd,off,size) fsync(fd)
#endif

/* ---- setproctitle() 支持 ---- */
#if (defined __NetBSD__ || defined __FreeBSD__ || defined __OpenBSD__)
#define USE_SETPROCTITLE  /**< BSD：原生支持 setproctitle() */
#endif

#if defined(__HAIKU__)
#define ESOCKTNOSUPPORT 0  /**< Haiku：补充缺失的错误码 */
#endif

#if (defined __linux || defined __APPLE__)
#define USE_SETPROCTITLE           /**< Linux/macOS：使用自定义 setproctitle 实现 */
#define INIT_SETPROCTITLE_REPLACEMENT  /**< 标记需要初始化 setproctitle 替换实现 */
/**
 * @brief 初始化进程标题替换实现（Linux/macOS）
 * @param argc 主函数参数数量
 * @param argv 主函数参数数组
 */
void spt_init(int argc, char *argv[]);
/**
 * @brief 设置进程标题（在 ps/top 中显示的进程名）
 * @param fmt printf 格式化字符串
 * @param ... 可变参数
 */
void setproctitle(const char *fmt, ...);
#endif

/* ---- 字节序检测 ---- */
#include <sys/types.h> /* 可能定义 BYTE_ORDER */

#ifndef BYTE_ORDER
#if (BSD >= 199103)
# include <machine/endian.h>
#else
#if defined(linux) || defined(__linux__)
# include <endian.h>
#else
#define	LITTLE_ENDIAN	1234	/**< 小端：低有效字节在低地址（x86、PC） */
#define	BIG_ENDIAN	4321	/**< 大端：高有效字节在低地址（IBM、网络字节序） */
#define	PDP_ENDIAN	3412	/**< PDP 端：字内小端、字间大端 */

#if defined(__i386__) || defined(__x86_64__) || defined(__amd64__) || \
   defined(vax) || defined(ns32000) || defined(sun386) || \
   defined(MIPSEL) || defined(_MIPSEL) || defined(BIT_ZERO_ON_RIGHT) || \
   defined(__alpha__) || defined(__alpha)
#define BYTE_ORDER    LITTLE_ENDIAN
#endif

#if defined(sel) || defined(pyr) || defined(mc68000) || defined(sparc) || \
    defined(is68k) || defined(tahoe) || defined(ibm032) || defined(ibm370) || \
    defined(MIPSEB) || defined(_MIPSEB) || defined(_IBMR2) || defined(DGUX) ||\
    defined(apollo) || defined(__convex__) || defined(_CRAY) || \
    defined(__hppa) || defined(__hp9000) || \
    defined(__hp9000s300) || defined(__hp9000s700) || \
    defined (BIT_ZERO_ON_LEFT) || defined(m68k) || defined(__sparc)
#define BYTE_ORDER	BIG_ENDIAN
#endif
#endif /* linux */
#endif /* BSD */
#endif /* BYTE_ORDER */

/* 处理 __BYTE_ORDER 有但 BYTE_ORDER 未定义的情况 */
#ifndef BYTE_ORDER
#ifdef __BYTE_ORDER
#if defined(__LITTLE_ENDIAN) && defined(__BIG_ENDIAN)
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN __LITTLE_ENDIAN
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN __BIG_ENDIAN
#endif
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
#define BYTE_ORDER LITTLE_ENDIAN
#else
#define BYTE_ORDER BIG_ENDIAN
#endif
#endif
#endif
#endif

#if !defined(BYTE_ORDER) || \
    (BYTE_ORDER != BIG_ENDIAN && BYTE_ORDER != LITTLE_ENDIAN)
	/* 若 BYTE_ORDER 未定义或无效，强制编译报错，要求修复上方宏 */
#error "Undefined or invalid BYTE_ORDER"
#endif

/* ---- GCC __sync 原子操作支持检测（x86/amd64/powerpc） ---- */
#if (__i386 || __amd64 || __powerpc__) && __GNUC__
#define GNUC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if defined(__clang__)
#define HAVE_ATOMIC  /**< Clang：支持 __sync 原子操作 */
#endif
#if (defined(__GLIBC__) && defined(__GLIBC_PREREQ))
#if (GNUC_VERSION >= 40100 && __GLIBC_PREREQ(2, 6))
#define HAVE_ATOMIC  /**< GCC 4.1+ / glibc 2.6+：支持 __sync 原子操作 */
#endif
#endif
#endif

/* ---- ARM 平台宏统一 ---- */
#if defined(__arm) && !defined(__arm__)
#define __arm__  /**< 统一 ARM 检测宏 */
#endif
#if defined (__aarch64__) && !defined(__arm64__)
#define __arm64__  /**< 统一 ARM64 检测宏 */
#endif

/* ---- SPARC 平台宏统一 ---- */
#if defined(__sparc) && !defined(__sparc__)
#define __sparc__  /**< 统一 SPARC 检测宏 */
#endif

/* ---- 对齐访问要求检测 ---- */
#if defined(__sparc__) || defined(__arm__)
#define USE_ALIGNED_ACCESS  /**< SPARC/ARM：要求对齐内存访问（禁止非对齐读写） */
#endif

/* ---- 线程名称设置宏 ---- */
/**
 * @brief 设置当前线程名称（在 ps/top 中显示）
 *        各平台 API 不同，此宏统一抽象。
 * @param name 线程名称字符串
 */
#ifdef __linux__
#define latte_set_thread_title(name) pthread_setname_np(pthread_self(), name)
#else
#if (defined __FreeBSD__ || defined __OpenBSD__)
#include <pthread_np.h>
#define latte_set_thread_title(name) pthread_set_name_np(pthread_self(), name)
#elif defined __NetBSD__
#include <pthread.h>
#define latte_set_thread_title(name) pthread_setname_np(pthread_self(), "%s", name)
#elif defined __HAIKU__
#include <kernel/OS.h>
#define latte_set_thread_title(name) rename_thread(find_thread(0), name)
#else
#if (defined __APPLE__ && defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 1070)
int pthread_setname_np(const char *name);
#include <pthread.h>
#define latte_set_thread_title(name) pthread_setname_np(name)
#else
#define latte_set_thread_title(name)  /**< 不支持的平台：空操作 */
#endif
#endif
#endif

/* ---- CPU 亲和性设置支持 ---- */
#if (defined __linux || defined __NetBSD__ || defined __FreeBSD__ || defined __DragonFly__)
#define USE_SETCPUAFFINITY  /**< 平台支持 setcpuaffinity() */
/**
 * @brief 将当前线程绑定到指定 CPU 核心（CPU 亲和性）
 * @param cpulist CPU 列表字符串（如 "0,1,2-3"）
 */
void setcpuaffinity(const char *cpulist);
#endif

/* ---- posix_fadvise() 支持检测 ---- */
#if defined(__linux__) || __FreeBSD__ >= 10
#define HAVE_FADVISE  /**< 平台支持 posix_fadvise()（文件预读取提示） */
#endif

/* ---- POPCNT 指令支持检测（x86_64） ---- */
#if defined(__x86_64__) && ((defined(__GNUC__) && __GNUC__ > 5) || (defined(__clang__)))
    #if defined(__has_attribute) && __has_attribute(target)
        #define HAVE_POPCNT
        /** @brief 启用 POPCNT 指令支持的函数属性 */
        #define ATTRIBUTE_TARGET_POPCNT __attribute__((target("popcnt")))
    #else
        #define ATTRIBUTE_TARGET_POPCNT
    #endif
#else
    #define ATTRIBUTE_TARGET_POPCNT
#endif

/* ---- AVX2 指令支持检测（x86_64） ---- */
#if defined (__x86_64__) && ((defined(__GNUC__) && __GNUC__ >= 5) || (defined(__clang__) && __clang_major__ >= 4))
#if defined(__has_attribute) && __has_attribute(target)
#define HAVE_AVX2  /**< 平台支持 AVX2 指令集 */
#endif
#endif

#if defined (HAVE_AVX2)
/** @brief 启用 AVX2 指令支持的函数属性 */
#define ATTRIBUTE_TARGET_AVX2 __attribute__((target("avx2")))
#else
#define ATTRIBUTE_TARGET_AVX2
#endif

#endif
