#include "latte_signal.h"
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include "log/log.h"
#include <errno.h>

// 阻塞默认信号集
void block_default_signals(sigset_t *signal_set, sigset_t *old_set) {
    sigemptyset(signal_set); // 清空信号集
#ifndef DEBUG
    // SIGINT 会影响我们的gdb调试
    sigaddset(signal_set, SIGINT);
#endif
    sigaddset(signal_set, SIGTERM); // 添加 SIGTERM 到信号集中
    sigaddset(signal_set, SIGUSR1); // 添加 SIGUSR1 到信号集中
    pthread_sigmask(SIG_BLOCK, signal_set, old_set); // 阻塞信号集
}

// 解除阻塞默认信号集
void unblock_default_signals(sigset_t *signal_set, sigset_t *old_set) {
    sigemptyset(signal_set); // 清空信号集
#ifndef DEBUG
    sigaddset(signal_set, SIGINT); // 添加 SIGINT 到信号集中
#endif
    sigaddset(signal_set, SIGTERM); // 添加 SIGTERM 到信号集中
    sigaddset(signal_set, SIGUSR1); // 添加 SIGUSR1 到信号集中
    pthread_sigmask(SIG_UNBLOCK, signal_set, old_set); // 解除阻塞信号集
}

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

void  start_wait_for_signals(sigset_t *signal_set) {
  pthread_t pThread;
  pthread_attr_t pThreadAttrs;

  pthread_attr_init(&pThreadAttrs);
  pthread_attr_setdetachstate(&pThreadAttrs, PTHREAD_CREATE_DETACHED);

  pthread_create(&pThread, &pThreadAttrs, wait_for_signals, (void *)signal_set);
}

void set_signal_handler0(int sig, sighandler_t func) {
  struct sigaction newsa, oldsa;
  sigemptyset(&newsa.sa_mask);
  newsa.sa_flags   = 0;
  newsa.sa_handler = func;

  int rc = sigaction(sig, &newsa, &oldsa);
  if (rc) {
     log_error("latte_lib","Failed to set signal %d ",sig);
  }
}

void set_signal_handler(sighandler_t func)
{
  set_signal_handler0(SIGQUIT, func);
  set_signal_handler0(SIGINT, func);
  set_signal_handler0(SIGHUP, func);
  set_signal_handler0(SIGTERM, func);
  signal(SIGPIPE, SIG_IGN);
}