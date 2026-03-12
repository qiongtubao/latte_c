/**
 * @file env.h
 * @brief 文件系统环境接口（Env）
 *        封装文件锁、文件读写、文件管理等操作，提供线程安全的文件系统抽象层。
 *        使用互斥锁（set_lock）和文件锁集合（set）管理并发文件访问。
 */

#ifndef __LATTE_ENV_H
#define __LATTE_ENV_H
#include "set/set.h"
#include "mutex/mutex.h"
#include "error/error.h"
#include "file.h"
#include "sds/sds.h"

/**
 * @brief 文件系统环境结构体
 *        管理文件锁集合，确保多线程环境下文件操作的安全性。
 */
typedef struct Env {
    latte_mutex_t* set_lock; /**< 互斥锁，保护 set 的并发访问 */
    set_t* set;              /**< 已锁定文件锁的集合，防止重复锁定 */
} Env;

/**
 * @brief 对指定文件加锁
 *        在 env 的文件锁集合中注册文件锁，防止并发写冲突。
 * @param env      Env 实例指针
 * @param filename 要加锁的文件名（sds 字符串）
 * @param lock     输出：成功时设置为新建的 FileLock 指针
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* envLockFile(Env* env, sds_t filename, FileLock** lock);

/**
 * @brief 对指定文件解锁
 *        从 env 的文件锁集合中移除并释放文件锁。
 * @param env  Env 实例指针
 * @param lock 要释放的 FileLock 指针
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* envUnlockFile(Env* env, FileLock* lock);

/**
 * @brief 创建新的 Env 实例
 *        分配并初始化互斥锁和文件锁集合。
 * @return 新建的 Env 指针；内存分配失败返回 NULL
 */
Env* envCreate();

/**
 * @brief 释放 Env 实例
 *        销毁互斥锁和文件锁集合，释放 Env 本身。
 * @param env 要释放的 Env 指针（NULL 时安全跳过）
 */
void envRelease(Env* env);

/**
 * @brief 释放可写文件对象
 *        关闭文件并释放 WritableFile 资源。
 * @param env  Env 实例指针
 * @param file 要释放的 WritableFile 指针
 */
void envWritableFileRelease(Env* env, WritableFile* file);

/**
 * @brief 创建可写文件
 *        打开或创建指定文件，返回可追加写入的 WritableFile 对象。
 * @param env      Env 实例指针
 * @param filename 目标文件路径（sds 字符串）
 * @param file     输出：成功时设置为新建的 WritableFile 指针
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* envWritableFileCreate(Env* env, sds_t filename, WritableFile** file);

/**
 * @brief 删除指定文件
 * @param env      Env 实例指针
 * @param filename 要删除的文件路径（sds 字符串）
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* envRemoveFile(Env* env, sds_t filename);

/**
 * @brief 重命名文件
 * @param env     Env 实例指针
 * @param oldname 原文件路径（sds 字符串）
 * @param newname 新文件路径（sds 字符串）
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* envRenameFile(Env* env, sds_t oldname, sds_t newname);

/**
 * @brief 将 sds 数据同步写入文件（原子性）
 *        写入完成后调用 fsync 确保数据落盘。
 * @param env   Env 实例指针
 * @param fname 目标文件路径（sds 字符串）
 * @param data  要写入的 sds 数据
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* envWriteSdsToFileSync(Env* env,
                             sds_t fname, sds_t data);

/**
 * @brief 创建顺序读文件对象
 * @param env      Env 实例指针
 * @param filename 目标文件路径（sds 字符串）
 * @param file     输出：成功时设置为新建的 SequentialFile 指针
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* envSequentialFileCreate(Env* env, sds_t filename, SequentialFile** file);

/**
 * @brief 读取文件全部内容到 sds 字符串
 * @param env   Env 实例指针
 * @param fname 源文件路径（sds 字符串）
 * @param data  输出：成功时设置为包含文件内容的 sds 字符串（调用方负责 sdsfree）
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* envReadFileToSds(Env* env, sds_t fname, sds* data);
#endif
