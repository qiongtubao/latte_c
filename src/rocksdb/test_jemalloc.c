#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void allocate_memory(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    // 初始化内存以防止优化器移除分配
    memset(ptr, 0, size);
    free(ptr); // 立即释放内存以模拟实际应用场景中的分配和释放
}

int main() {
    // 注意：这里不直接设置环境变量，而是在运行时通过命令行或 shell 设置

    // 分配一些内存
    for (int i = 0; i < 1000; ++i) {
        allocate_memory(1024 * 1024); // 分配 1MB 内存
    }

    // 模拟一段时间的工作负载
    sleep(5);

    return 0;
}