/*
 * testassert.h - test 模块头文件
 * 
 * Latte C 库组件
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */


#include "testhelp.h"

#ifndef __TESTASSERT_H
#define __TESTASSERT_H

#define assert(e) do {							\
	if (!(e)) {				\
		printf(						\
		    "%s:%d: Failed assertion: \"%s\"\n",	\
		    __FILE__, __LINE__, #e);				\
		return 0;						\
	}								\
} while (0)

#endif