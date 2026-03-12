#ifndef LATTE_C_SIGNAL_H
#define LATTE_C_SIGNAL_H

#include <signal.h>
#ifdef __linux__
    #include <bits/types/sigset_t.h>
    #include <bits/sigaction.h>
#elif defined(__APPLE__) && defined(__MACH__)

#else

#endif

/**
 * @brief 阻塞默认信号集（SIGTERM、SIGUSR1，非 DEBUG 模式下包含 SIGINT）
 * 通常在主线程或工作线程启动前调用，防止信号打断业务逻辑。
 * @param signal_set 输出参数，返回构建的信号集
 * @param old_set    输出参数，保存调用前的旧信号掩码（可用于恢复）
 */
void block_default_signals(sigset_t *signal_set, sigset_t *old_set);

/**
 * @brief 解除默认信号集的阻塞
 * 与 block_default_signals 配对使用，恢复信号处理。
 * @param signal_set 输出参数，返回构建的信号集
 * @param old_set    输出参数，保存调用前的旧信号掩码
 */
void unblock_default_signals(sigset_t *signal_set, sigset_t *old_set);

/**
 * @brief 启动一个独立线程等待信号集中的信号
 * 线程以 detached 方式运行，收到信号后记录日志（当前实现为无限循环）。
 * @param signal_set 要等待的信号集指针（由 block_default_signals 构建）
 */
void start_wait_for_signals(sigset_t *signal_set);

/** @brief 信号处理函数类型，接收信号编号 */
typedef void (*sighandler_t)(int);

/**
 * @brief 为默认信号组（SIGQUIT/SIGINT/SIGHUP/SIGTERM）设置统一处理函数，
 * 同时将 SIGPIPE 设置为 SIG_IGN（忽略管道断裂信号）
 * @param func 信号处理函数，传 NULL 可恢复默认处理
 */
void set_signal_handler(sighandler_t func);

/**
 * @brief 为指定信号设置处理函数
 * @param sig  信号编号（如 SIGTERM、SIGINT）
 * @param func 信号处理函数
 */
void set_signal_handler0(int sig, sighandler_t func);

#endif