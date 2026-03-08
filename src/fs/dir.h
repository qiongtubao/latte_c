/*
 * dir.h - fs 模块头文件
 * 
 * Latte C 库组件
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef __LATTE_DIR_H
#define __LATTE_DIR_H

#include "error/error.h"
#include "iterator/iterator.h"

Error* dirCreate(char* path);
Error* dirCreateRecursive(const char* path, mode_t mode);
int dirIs(char* path);
latte_iterator_t* dir_scan_file(char* dir_path, const char* filter_pattern);

#endif