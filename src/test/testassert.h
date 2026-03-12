/**
 * @file testassert.h
 * @brief 测试断言宏
 *        提供轻量级测试断言，断言失败时打印文件名、行号和表达式并从当前函数返回 0。
 *        需要先包含 testhelp.h。
 */

#include "testhelp.h"

#ifndef __TESTASSERT_H
#define __TESTASSERT_H

/**
 * @brief 测试断言宏
 *        若表达式 e 为假（0），打印失败位置信息并从当前函数 return 0。
 *        通常在返回 int 的测试函数中使用。
 * @param e 断言表达式；为真时继续执行，为假时打印并 return 0
 */
#define assert(e) do {							\
	if (!(e)) {				\
		printf(						\
		    "%s:%d: Failed assertion: \"%s\"\n",	\
		    __FILE__, __LINE__, #e);				\
		return 0;						\
	}								\
} while (0)

#endif
