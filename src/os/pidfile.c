#include "pidfile.h"


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <string.h>


static char *pid_path = NULL; // 静态局部变量，保存PID路径
// 获取PID路径
const char *getPidPath() {
    return pid_path;
}

// 假设 setPidPath 和 getPidPath 已经定义好了
void setPidPath(const char *progName) {
    pid_path = progName;
}



int writePidFile(const char *progName) {
    assert(progName != NULL);

    setPidPath(progName);
    const char *path = getPidPath();
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

void removePidFile(void) {
    if (pid_path != NULL) {
        unlink(pid_path);
        setPidPath(NULL);
    }
}