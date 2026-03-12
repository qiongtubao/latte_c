/**
 * @file dir.h
 * @brief 目录操作接口
 *        提供目录创建、判断及文件扫描功能。
 */
#ifndef __LATTE_DIR_H
#define __LATTE_DIR_H

#include "error/error.h"
#include "iterator/iterator.h"

/**
 * @brief 创建单级目录
 *        等同于 mkdir(path, 默认权限)，若目录已存在则返回成功。
 * @param path 要创建的目录路径
 * @return 成功返回 Ok（&Ok）；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* dirCreate(char* path);

/**
 * @brief 递归创建多级目录（类似 mkdir -p）
 *        若中间路径不存在则逐级创建，已存在的目录不报错。
 * @param path 要递归创建的目录路径
 * @param mode 目录权限模式（如 0755）
 * @return 成功返回 Ok；失败返回新建的 Error 对象（调用方负责释放）
 */
Error* dirCreateRecursive(const char* path, mode_t mode);

/**
 * @brief 判断指定路径是否为目录
 * @param path 要判断的路径字符串
 * @return 是目录返回 1；不是目录或路径不存在返回 0
 */
int dirIs(char* path);

/**
 * @brief 扫描目录下符合过滤模式的文件，返回迭代器
 *        迭代器每次调用 latte_iterator_next 返回一个匹配文件名的 char* 指针。
 * @param dir_path       要扫描的目录路径
 * @param filter_pattern 文件名过滤模式（如 "*.log"，NULL 表示匹配所有文件）
 * @return 新建的 latte_iterator_t 指针；失败或目录为空返回 NULL
 */
latte_iterator_t* dir_scan_file(char* dir_path, const char* filter_pattern);

#endif
