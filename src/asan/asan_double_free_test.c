
/**
 *  主进程
 *    2次释放同一个对象
 */
#include <stdlib.h>

void doubleFree() {
    int *array = (int *)malloc(10 * sizeof(int));
    
    // 使用 array...
    
    free(array); // 第一次释放
    
    // 某些操作后，错误地再次释放相同的指针
    free(array); // 第二次释放（导致 double free 错误）
}

int main() {
    doubleFree();
    return 0;
}