#ifndef LATTE_C_PROCESS_H
#define LATTE_C_PROCESS_H
#include "sds/sds.h"
#include "log/log.h"

/**
 * @brief 将相对路径或文件名转换为绝对路径
 * 支持处理路径头部的 "../" 序列，但不做完整路径规范化。
 * @param filename 文件名或相对路径（已是绝对路径则原样返回）
 * @return sds_t 返回绝对路径的 SDS 字符串，失败返回 NULL（调用方负责 sds_delete）
 */
sds_t getAbsolutePath(char *filename);

/**
 * @brief 从完整程序路径中提取进程名（去除目录前缀）
 * @param prog_full_name 程序完整路径（如 "/usr/bin/myapp"）
 * @return sds_t 返回进程名的 SDS 字符串（如 "myapp"，调用方负责 sds_delete）
 */
sds_t get_process_name(const char *prog_full_name);

/**
 * @brief 将进程守护化（daemonize），并重定向标准输出/标准错误到文件
 * 内部调用 daemon() 使进程脱离终端，然后重定向 stdout/stderr。
 * @param std_out_file 标准输出重定向目标文件路径
 * @param std_err_file 标准错误重定向目标文件路径
 * @return int 成功返回 0，失败返回非 0
 */
int daemonize_service(sds_t std_out_file, sds_t std_err_file);

/**
 * @brief 将 stdin 重定向到 /dev/null，stdout/stderr 重定向到指定文件
 * 使用 O_TRUNC 清空目标文件旧内容，并关闭缓冲以确保实时输出。
 * @param std_out_file 标准输出目标文件路径
 * @param std_err_file 标准错误目标文件路径
 */
void sys_log_redirect(const char *std_out_file, const char *std_err_file);

#endif