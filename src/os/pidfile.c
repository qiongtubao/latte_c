#include "pidfile.h"


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <string.h>


static char *pid_path = NULL; // 静态局部变量，保存PID路径
// 获取PID路径
const char *get_pid_file() {
    return pid_path;
}

// 假设 setPidPath 和 getPidPath 已经定义好了
void set_pid_file(const char *progName) {
    pid_path = progName;
}



int write_pid_file(const char *progName) {
    assert(progName != NULL);

    set_pid_file(progName);
    const char *path = get_pid_file();
    FILE *file = fopen(path, "w"); // 以写入模式打开文件，覆盖旧内容
    int rv = -1;

    if (file != NULL) {
        // 写入当前进程ID
        if (fprintf(file, "%d\n", getpid()) != EOF) {
            rv = 0;
        } else {
            perror("Error writing to PID file");
        }
        fclose(file); // 关闭文件
    } else {
        perror("Error opening PID file");
    }

    return rv;
}

void remove_pid_file(void) {
    if (pid_path != NULL) {
        unlink(pid_path);
        set_pid_file(NULL);
    }
}