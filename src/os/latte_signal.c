#include "latte_signal.h"
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include "log/log.h"
#include <errno.h>

/**
 * @brief 阻塞默认信号集，防止信号打断业务线程
 * 非 DEBUG 模式下额外阻塞 SIGINT（防止影响 gdb 调试）。
 * 总是阻塞 SIGTERM 和 SIGUSR1。
 * @param signal_set 输出参数，构建的信号集
 * @param old_set    输出参数，调用前的旧信号掩码
 */
void block_default_signals(sigset_t *signal_set, sigset_t *old_set) {
    sigemptyset(signal_set); /* 清空信号集 */
#ifndef DEBUG
    /* SIGINT 会影响 gdb 调试，非 DEBUG 模式才阻塞 */
    sigaddset(signal_set, SIGINT);
#endif
    sigaddset(signal_set, SIGTERM); /* 添加优雅退出信号 */
    sigaddset(signal_set, SIGUSR1); /* 添加用户自定义信号1 */
    pthread_sigmask(SIG_BLOCK, signal_set, old_set); /* 在当前线程阻塞信号集 */
}

/**
 * @brief 解除默认信号集的阻塞，恢复信号处理
 * @param signal_set 输出参数，构建的信号集
 * @param old_set    输出参数，调用前的旧信号掩码
 */
void unblock_default_signals(sigset_t *signal_set, sigset_t *old_set) {
    sigemptyset(signal_set); /* 清空信号集 */
#ifndef DEBUG
    sigaddset(signal_set, SIGINT);
#endif
    sigaddset(signal_set, SIGTERM);
    sigaddset(signal_set, SIGUSR1);
    pthread_sigmask(SIG_UNBLOCK, signal_set, old_set); /* 解除阻塞 */
}

/**
 * @brief 信号等待线程函数，循环等待信号集中的信号并记录日志
 * 收到信号后记录日志，然后继续等待（无限循环）。
 * 若 sigwait 返回错误则记录错误日志。
 * @param args 指向 sigset_t 的指针，由 start_wait_for_signals 传入
 * @return void* 始终返回 NULL
 */
void* wait_for_signals(void *args) {
  log_info("latte_lib", "Start thread to wait signals.");
  sigset_t *signal_set = (sigset_t *) args;
  int sig_number = -1;
  while (1) {
    errno = 0;
    int ret = sigwait(signal_set, &sig_number);
    if (ret != 0) {
      log_error("latte_lib", "sigwait return value: %d, %d \n", ret, sig_number);
    }
  }
  return NULL;
}

/**
 * @brief 启动一个 detached 线程专门等待信号集中的信号
 * 线程以 PTHREAD_CREATE_DETACHED 方式运行，无需 join。
 * @param signal_set 要等待的信号集指针（由 block_default_signals 构建）
 */
void start_wait_for_signals(sigset_t *signal_set) {
  pthread_t pThread;
  pthread_attr_t pThreadAttrs;

  pthread_attr_init(&pThreadAttrs);
  /* 设置线程为 detached 模式，退出时自动释放资源 */
  pthread_attr_setdetachstate(&pThreadAttrs, PTHREAD_CREATE_DETACHED);

  pthread_create(&pThread, &pThreadAttrs, wait_for_signals, (void *)signal_set);
}

/**
 * @brief 为指定信号设置处理函数（使用 sigaction）
 * 清空 sa_mask，设置 sa_flags=0（不使用 SA_RESTART 等标志）。
 * @param sig  信号编号
 * @param func 信号处理函数
 */
void set_signal_handler0(int sig, sighandler_t func) {
  struct sigaction newsa, oldsa;
  sigemptyset(&newsa.sa_mask);
  newsa.sa_flags   = 0;
  newsa.sa_handler = func;

  int rc = sigaction(sig, &newsa, &oldsa);
  if (rc) {
     log_error("latte_lib", "Failed to set signal %d ", sig);
  }
}

/**
 * @brief 为默认信号组统一设置处理函数，SIGPIPE 设为忽略
 * 覆盖的信号：SIGQUIT、SIGINT、SIGHUP、SIGTERM；
 * SIGPIPE 设置为 SIG_IGN 防止写已关闭的管道/socket 导致进程退出。
 * @param func 信号处理函数，传 NULL 可恢复默认处理
 */
void set_signal_handler(sighandler_t func)
{
  set_signal_handler0(SIGQUIT, func);
  set_signal_handler0(SIGINT,  func);
  set_signal_handler0(SIGHUP,  func);
  set_signal_handler0(SIGTERM, func);
  signal(SIGPIPE, SIG_IGN); /* 忽略 SIGPIPE，防止写失败导致进程退出 */
}