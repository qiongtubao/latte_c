/*
 * lzf_test.c - lzf 模块实现文件
 * 
 * Latte C 库组件实现
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

//
// Created by dong on 23-5-22.
//

#include "test/testhelp.h"
#include "test/testassert.h"

#include <stdio.h>
#include "zmalloc/zmalloc.h"
#include "lzfP.h"

int test_lzf() {
    return 1;
}

int test_api(void) {
    {
#ifdef LATTE_TEST
        // ..... private
#endif
        test_cond("lzf function",
                  test_lzf() == 1);

    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}