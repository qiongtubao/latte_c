#include "process.h"
#include <string.h>
#include "log/log.h"
#include "utils/utils.h"
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils/utils.h"


/**
 * @brief 从完整路径中提取最后一个 '/' 后的文件名部分
 * 若路径中不含 '/' 则返回整个路径字符串。
 * @param path 完整路径字符串
 * @return char* 指向文件名起始位置的指针（不拥有内存）
 */
char *basename(const char *path) {
    const char *slash = strrchr(path, '/'); /* 找最后一个 '/' */
    if (slash != NULL) {
        return (char *)(slash + 1); /* 返回 '/' 之后的部分 */
    } else {
        /* 如果没有斜杠，则返回整个路径 */
        return (char *)path;
    }
}

/**
 * @brief 从完整程序路径中提取进程名（去除目录前缀）
 * 将路径复制到栈缓冲区后调用 basename 提取文件名，
 * 再包装为 SDS 字符串返回（调用方负责 sds_delete）。
 * @param prog_name 程序完整路径（如 "/usr/bin/myapp"）
 * @return sds_t 进程名的 SDS 字符串（如 "myapp"），失败返回空字符串
 */
sds_t get_process_name(const char *prog_name) {
  sds_t process_name;
  int buf_len = strlen(prog_name);
  /* 使用栈上可变长数组存储路径副本，避免修改原字符串 */
  char buf[buf_len + 1];
  if (buf == NULL) {
    log_error("latte_lib-os","Failed to alloc memory for program name.");
    return "";
  }
  snprintf(buf, buf_len + 1, "%s", prog_name); /* 复制路径到缓冲区 */

  process_name = basename(buf); /* 提取文件名部分 */
  return sds_new(process_name); /* 包装为 SDS 返回 */
}


/**
 * @brief 将进程守护化的内部实现
 * 调用 POSIX daemon() 使进程脱离控制终端：
 *   nochdir=1  保持当前工作目录不变
 *   noclose=0  关闭标准 I/O（close_std_streams=true 时）
 * macOS 上 daemon() 已废弃，使用编译器 pragma 压制警告。
 * @param close_std_streams true 时关闭标准 I/O，false 时保留
 * @return int 成功返回 0，daemon() 失败返回非 0
 */
int daemonize_service0(bool close_std_streams) {
    int nochdir = 1;                              /* 保持当前工作目录 */
    int noclose = close_std_streams ? 0 : 1;     /* 是否关闭标准 I/O */
#ifdef __MACH__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  int rc = daemon(nochdir, noclose);
#ifdef __MACH__
#pragma GCC diagnostic pop
#endif
    /* 此处已在子进程中运行，父进程已退出，setsid() 已被调用 */
  if (rc != 0) {
    log_error("latte_lib", "Error: unable to daemonize: %s \n" ,strerror(errno));
  }
  return rc;
}

/** @brief 文件权限掩码：所有者读写，组读，其他读 */
#define RWRR (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

/**
 * @brief 将 stdin 重定向到 /dev/null，stdout/stderr 重定向到指定文件
 * 步骤：
 *   1. 将 stdin 重定向到 /dev/null（避免守护进程读取终端）
 *   2. 获取当前时间戳（用于日志记录）
 *   3. 以 O_TRUNC 模式打开 stderr 目标文件，dup2 到 STDERR_FILENO
 *   4. 以 O_TRUNC 模式打开 stdout 目标文件，dup2 到 STDOUT_FILENO
 *   5. 关闭标准 I/O 缓冲，保证实时输出
 * @param std_out_file 标准输出目标文件路径（相对或绝对路径）
 * @param std_err_file 标准错误目标文件路径（相对或绝对路径）
 */
void sys_log_redirect(const char *std_out_file, const char *std_err_file)
{
  int rc = 0;

  /* 将 stdin 重定向到 /dev/null，避免守护进程读取终端输入 */
  int nullfd = open("/dev/null", O_RDONLY);
  if (nullfd >= 0) {
    dup2(nullfd, STDIN_FILENO);
    close(nullfd);
  }

  /* 获取当前时间戳，用于日志中记录重定向时间 */
  struct timeval tv;
  rc = gettimeofday(&tv, NULL);
  if (rc != 0) {
    log_error("latte_lib", "Fail to get current time");
    tv.tv_sec = 0;
  }

  int std_err_flag, std_out_flag;
  /* 使用创建+追加+写入模式，文件不存在时自动创建 */
  std_err_flag = std_out_flag = O_CREAT | O_APPEND | O_WRONLY;

  /* 将相对路径转换为绝对路径 */
  sds_t err_file = getAbsolutePath(std_err_file);

  /* 清空旧内容（O_TRUNC），避免历史日志干扰 */
  std_err_flag |= O_TRUNC;

  /* 打开 stderr 目标文件并重定向 STDERR_FILENO */
  int errfd = open(err_file, std_err_flag, RWRR);
  if (errfd >= 0) {
    dup2(errfd, STDERR_FILENO);
    close(errfd);
  }
  setvbuf(stderr, NULL, _IONBF, 0); /* 关闭 stderr 缓冲，保证实时写入 */
  log_error("latte_lib", "Process %d built error output at %lld ", getpid() , tv.tv_sec );

  sds_t outFile = getAbsolutePath(std_out_file);

  /* 清空旧内容（O_TRUNC） */
  std_out_flag |= O_TRUNC;

  /* 打开 stdout 目标文件并重定向 STDOUT_FILENO */
  int outfd = open(outFile, std_out_flag, RWRR);
  if (outfd >= 0) {
    dup2(outfd, STDOUT_FILENO);
    close(outfd);
  }
  setvbuf(stdout, NULL, _IONBF, 0); /* 关闭 stdout 缓冲 */
  log_error("latte_lib", "Process %d  built standard output at %lld", getpid(),  tv.tv_sec);
  return;
}


/**
 * @brief 将进程守护化并重定向标准输出/错误到指定文件
 * 先调用 daemonize_service0(false) 使进程脱离终端（不关闭标准 I/O），
 * 再调用 sys_log_redirect 将 stdout/stderr 重定向到文件。
 * @param stdout 标准输出重定向目标文件路径
 * @param stderr 标准错误重定向目标文件路径
 * @return int 成功返回 0，daemonize 失败返回非 0
 */
int daemonize_service(sds_t stdout, sds_t stderr) {
    int rc = daemonize_service0(false); /* 守护化，保留标准 I/O 供后续重定向 */
    if (rc != 0) {
        log_error("latte_lib", "Error: \n");
        return rc;
    }
    sys_log_redirect(stdout, stderr); /* 将标准 I/O 重定向到文件 */
    return 0;
}

/**
 * @brief 将相对路径或文件名转换为绝对路径（SDS 字符串）
 *
 * 处理逻辑：
 *   1. 先修剪路径两端的空白/回车/换行/制表符
 *   2. 若路径以 '/' 开头，视为绝对路径直接返回
 *   3. 否则获取当前工作目录（getcwd），拼接 '/' 后追加相对路径
 *   4. 对路径头部的每个 "../" 进行规范化：
 *      从 cwd 末尾删去一个目录层级，同时从 relpath 头部删去 "../"
 *      直到 cwd 为 "/" 或 relpath 不再以 "../" 开头
 *   5. 最终拼接 abspath + relpath 并释放 relpath
 *
 * @param filename 文件名或相对路径（可含 "../" 前缀）
 * @return sds_t 绝对路径的 SDS 字符串，失败返回 NULL（调用方负责 sds_delete）
 */
sds_t getAbsolutePath(char *filename) {
    char cwd[1024];
    sds_t abspath;
    sds_t relpath = sds_new(filename);

    relpath = sds_trim(relpath," \r\n\t"); /* 去除首尾空白字符 */
    if (relpath[0] == '/') return relpath; /* 已是绝对路径，直接返回 */

    /* 获取当前工作目录作为绝对路径基础 */
    if (getcwd(cwd,sizeof(cwd)) == NULL) {
        sds_delete(relpath);
        return NULL;
    }
    abspath = sds_new(cwd);
    /* 确保 abspath 以 '/' 结尾，便于后续拼接 */
    if (sds_len(abspath) && abspath[sds_len(abspath)-1] != '/')
        abspath = sds_cat(abspath,"/");

    /* 规范化路径头部的 "../" 序列：
     * 每遇到一个 "../" 就去掉 relpath 的前 3 个字符，
     * 同时从 abspath 末尾向前删去一个目录层级 */
    while (sds_len(relpath) >= 3 &&
           relpath[0] == '.' && relpath[1] == '.' && relpath[2] == '/')
    {
        sds_range(relpath,3,-1); /* 删去 relpath 头部的 "../" */
        if (sds_len(abspath) > 1) {
            /* 从 abspath 末尾倒数第 2 个字符向前找上一个 '/' */
            char *p = abspath + sds_len(abspath)-2;
            int trimlen = 1;

            while(*p != '/') {
                p--;
                trimlen++;
            }
            /* 删去最后一个目录层级（保留末尾的 '/'） */
            sds_range(abspath,0,-(trimlen+1));
        }
    }

    /* 将规范化后的相对路径拼接到绝对路径后面 */
    abspath = sds_cat_sds(abspath,relpath);
    sds_delete(relpath);
    return abspath;
}