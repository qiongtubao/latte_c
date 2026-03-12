#include "pidfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

/** @brief 全局静态变量，保存当前 PID 文件路径，NULL 表示未设置 */
static char *pid_path = NULL;

/**
 * @brief 获取当前记录的 PID 文件路径
 * @return const char* PID 文件路径，未设置时返回 NULL
 */
const char *get_pid_file() {
    return pid_path;
}

/**
 * @brief 设置（更新）PID 文件路径（内部辅助函数）
 * @param progName 新的 PID 文件路径，传 NULL 清空
 */
void set_pid_file(const char *progName) {
    pid_path = progName;
}

/**
 * @brief 将当前进程 PID 写入指定文件
 * 以写入模式（覆盖）打开文件，写入 "%d\n" 格式的 PID。
 * @param progName PID 文件路径（不能为 NULL）
 * @return int 成功返回 0，打开或写入失败返回 -1
 */
int write_pid_file(const char *progName) {
    assert(progName != NULL);

    /* 保存路径到全局变量 */
    set_pid_file(progName);
    const char *path = get_pid_file();
    FILE *file = fopen(path, "w"); /* 覆盖写入模式 */
    int rv = -1;

    if (file != NULL) {
        /* 写入当前进程 PID */
        if (fprintf(file, "%d\n", getpid()) != EOF) {
            rv = 0;
        } else {
            perror("Error writing to PID file");
        }
        fclose(file);
    } else {
        perror("Error opening PID file");
    }

    return rv;
}

/**
 * @brief 删除当前记录的 PID 文件，并清空内部 pid_path
 * 使用 unlink 删除文件，不检查删除是否成功（文件可能已不存在）。
 */
void remove_pid_file(void) {
    if (pid_path != NULL) {
        unlink(pid_path);
        set_pid_file(NULL); /* 清空路径记录 */
    }
}