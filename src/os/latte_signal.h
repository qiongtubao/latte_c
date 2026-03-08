/*
 * latte_signal.h - os 模块头文件
 * 
 * Latte C 库组件
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef LATTE_C_SIGNAL_H
#define LATTE_C_SIGNAL_H

#include <signal.h>
#ifdef __linux__
    #include <bits/types/sigset_t.h>
    #include <bits/sigaction.h>
#elif defined(__APPLE__) && defined(__MACH__)


#else

#endif 

void block_default_signals(sigset_t *signal_set, sigset_t *old_set);
void unblock_default_signals(sigset_t *signal_set, sigset_t *old_set);

void  start_wait_for_signals(sigset_t *signal_set);

typedef void (*sighandler_t)(int);
void set_signal_handler(sighandler_t func);
void set_signal_handler0(int sig, sighandler_t func);

#endif