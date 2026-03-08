/*
 * pidfile.h - os 模块头文件
 * 
 * Latte C 库组件
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef LATTE_C_PIDFILE_H
#define LATTE_C_PIDFILE_H
#include "sds/sds.h"

int write_pid_file(const char *progName);

//! Cleanup PID file for the current component
/**
 * Removes the PID file for the current component
 *
 */
void remove_pid_file(void);
const char* get_pid_file();
#endif