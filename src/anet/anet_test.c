/*
 * anet_test.c - anet 模块实现文件
 * 
 * Latte C 库组件实现
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "anet.h"

int test_anet(void) {
    char neterr[128];
    int fd = anetTcpServer(neterr, 6379, "127.0.0.1", 511);
    return 1;
}



int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("test_anet function", 
            test_anet() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}