/* This is a really minimal testing framework for C.
 *
 * Example:
 *
 * test_cond("Check if 1 == 1", 1==1)
 * test_cond("Check if 5 > 10", 5 > 10)
 * test_report()
 *
 * ----------------------------------------------------------------------------
 *
 * Copyright (c) 2010-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file testhelp.h
 * @brief 极简 C 测试框架
 *        提供 test_cond（执行单个测试并统计结果）和 test_report（汇总报告）两个宏，
 *        支持在任意 C 文件中快速编写测试用例，无需额外依赖。
 *
 * 使用示例：
 * @code
 *   test_cond("检查 1 == 1", 1 == 1)
 *   test_cond("检查 5 > 10", 5 > 10)
 *   test_report()
 * @endcode
 */
#ifndef __TESTHELP_H
#define __TESTHELP_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief 测试失败计数（内部状态，包含此头文件的编译单元各有独立实例） */
int __failed_tests = 0;
/** @brief 已执行测试总数（内部状态） */
int __test_num = 0;

/**
 * @brief 执行单个测试条件并打印结果
 *        递增测试计数，输出测试序号、描述和 PASSED/FAILED，失败时累计 __failed_tests。
 * @param descr 测试描述字符串（用于输出标识）
 * @param _c    测试条件表达式；非零为通过，零为失败
 */
#define test_cond(descr,_c) do { \
    __test_num++; printf("%d - %s: ", __test_num, descr); \
    if(_c) printf("PASSED\n"); else {printf("FAILED\n"); __failed_tests++;} \
} while(0);

/**
 * @brief 打印测试汇总报告
 *        输出总测试数、通过数、失败数；若有失败用例则打印警告并以状态码 1 退出。
 */
#define test_report() do { \
    printf("%d tests, %d passed, %d failed\n", __test_num, \
                    __test_num-__failed_tests, __failed_tests); \
    if (__failed_tests) { \
        printf("=== WARNING === We have failed tests here...\n"); \
        exit(1); \
    } \
} while(0);

#endif
