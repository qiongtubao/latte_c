/**
 * @file pidfile.h
 * @brief PID 文件管理接口
 *        提供写入、删除和查询当前进程 PID 文件的功能，
 *        用于守护进程管理（启动检测、停止信号等）。
 */
#ifndef LATTE_C_PIDFILE_H
#define LATTE_C_PIDFILE_H
#include "sds/sds.h"

/**
 * @brief 将当前进程的 PID 写入以 progName 为路径的文件
 * 写入格式为 "%d\n"，便于 shell 脚本读取。
 * @param progName PID 文件路径（同时作为内部 pid_path 保存）
 * @return int 成功返回 0，失败返回 -1
 */
int write_pid_file(const char *progName);

/**
 * @brief 删除当前已记录的 PID 文件，并清空内部 pid_path
 * 若 pid_path 为 NULL 则安全返回，不做任何操作。
 */
void remove_pid_file(void);

/**
 * @brief 获取当前记录的 PID 文件路径
 * @return const char* PID 文件路径字符串，未设置时返回 NULL
 */
const char* get_pid_file();
#endif