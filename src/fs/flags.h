/**
 * @file flags.h
 * @brief 文件系统 POSIX open 基础标志定义
 *        声明所有 POSIX open 操作共用的基础标志变量 kOpenBaseFlags。
 *        具体标志值（如 O_CLOEXEC）在对应平台的 .c 文件中定义。
 */
#ifndef __LATTE_FILE_FLAGS_H
#define __LATTE_FILE_FLAGS_H

/**
 * @brief 所有 POSIX open 操作的基础标志集合
 *        通常包含 O_CLOEXEC 等安全标志，确保 fork 后子进程不继承文件描述符。
 *        在 posix_file.c 或 file.c 中初始化，使用前应确保已完成平台检测。
 */
extern int kOpenBaseFlags;
// Common flags defined for all posix open operations



// // Define to 1 if you have a definition for fdatasync() in <unistd.h>.
// #if !defined(HAVE_FDATASYNC)
// #cmakedefine01 HAVE_FDATASYNC
// #endif  // !defined(HAVE_FDATASYNC)

// // Define to 1 if you have a definition for F_FULLFSYNC in <fcntl.h>.
// #if !defined(HAVE_FULLFSYNC)
// #cmakedefine01 HAVE_FULLFSYNC
// #endif  // !defined(HAVE_FULLFSYNC)

// // Define to 1 if you have a definition for O_CLOEXEC in <fcntl.h>.
// #if !defined(HAVE_O_CLOEXEC)
// #cmakedefine01 HAVE_O_CLOEXEC
// #endif  // !defined(HAVE_O_CLOEXEC)

// // Define to 1 if you have Google Snappy.
// #if !defined(HAVE_SNAPPY)
// #cmakedefine01 HAVE_SNAPPY
// #endif  // !defined(HAVE_SNAPPY)

#endif
