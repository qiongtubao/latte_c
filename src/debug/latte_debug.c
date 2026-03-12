
/**
 * @file latte_debug.c
 * @brief Latte 调试工具实现
 *        提供崩溃报告、堆栈跟踪、错误日志等调试功能
 */
#include "latte_debug.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <execinfo.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>

#include "log/log.h"

#include <pthread.h>
#include <string.h>

/* 全局变量 */
static int bug_report_start = 0; /**< 是否已记录错误报告头部标志 */
static pthread_mutex_t bug_report_start_mutex = PTHREAD_MUTEX_INITIALIZER; /**< 错误报告互斥锁 */


/**
 * @brief 开始错误报告记录
 *        使用互斥锁确保错误报告头部只被记录一次
 */
void bugReportStart(void) {
    pthread_mutex_lock(&bug_report_start_mutex);
    if (bug_report_start == 0) {
        LATTE_LIB_LOG(LOG_WARN,
        "\n\n=== LATTE BUG REPORT START: Cut & paste starting from here ===\n");
        bug_report_start = 1;
    }
    pthread_mutex_unlock(&bug_report_start_mutex);
}

/**
 * @brief 返回直接写入日志的文件描述符
 *        在关键代码段中可以直接使用 write() 系统调用写入日志
 *        当系统其他部分不可信时使用（如内存测试期间）
 * @return 文件描述符，当前返回 STDOUT_FILENO
 */
int openDirectLogFiledes(void) {
    // int log_to_stdout = logfile[0] == '\0';
    // int fd = log_to_stdout ?
    //     STDOUT_FILENO :
    //     open(logfile, O_APPEND|O_CREAT|O_WRONLY, 0644);
    // return fd;
    return STDOUT_FILENO;
}

/**
 * @brief 关闭通过 openDirectLogFiledes() 返回的文件描述符
 * @param fd 要关闭的文件描述符
 */
void closeDirectLogFiledes(int fd) {
    // int log_to_stdout = logfile[0] == '\0';
    // if (!log_to_stdout) close(fd);
}

/**
 * @brief 记录堆栈跟踪信息
 *        使用 backtrace() 调用获取堆栈信息，该函数设计为在信号处理器中安全调用
 * @param eip     可选的指令指针地址（可为 NULL）
 * @param uplevel 要跳过的调用函数层数
 */
void logStackTrace(void *eip, int uplevel) {
    void *trace[100];
    int trace_size = 0, fd = openDirectLogFiledes();
    char *msg;
    uplevel++; /* 跳过当前函数 */

    if (fd == -1) return; /* 如果无法记录日志则直接返回 */

    /* 首先获取堆栈跟踪！ */
    trace_size = backtrace(trace, 100);

    msg = "\n------ STACK TRACE ------\n";
    if (write(fd,msg,strlen(msg)) == -1) {/* 避免警告 */};

    if (eip) {
        /* 将 EIP 写入日志文件 */
        msg = "EIP:\n";
        if (write(fd,msg,strlen(msg)) == -1) {/* 避免警告 */};
        backtrace_symbols_fd(&eip, 1, fd);
    }

    /* 将符号信息写入日志文件 */
    msg = "\nBacktrace:\n";
    if (write(fd,msg,strlen(msg)) == -1) {/* 避免警告 */};
    backtrace_symbols_fd(trace+uplevel, trace_size-uplevel, fd);

    /* 清理资源 */
    closeDirectLogFiledes(fd);
}

/**
 * @brief 移除所有信号处理器
 *        将信号处理器重置为默认处理方式
 */
void removeSignalHandlers(void) {
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER | SA_RESETHAND;
    act.sa_handler = SIG_DFL;
    sigaction(SIGSEGV, &act, NULL);
    sigaction(SIGBUS, &act, NULL);
    sigaction(SIGFPE, &act, NULL);
    sigaction(SIGILL, &act, NULL);
    sigaction(SIGABRT, &act, NULL);
}

/**
 * @brief 结束错误报告记录
 *        输出报告结尾信息并根据参数决定如何终止程序
 * @param killViaSignal 是否通过信号终止程序
 * @param sig           终止信号类型
 */
void bugReportEnd(int killViaSignal, int sig) {
    struct sigaction act;

    LATTE_LIB_LOG(LOG_WARN,
"\n=== LATTE BUG REPORT END. Make sure to include from START to END. ===\n\n"
);

    /* 不要在可能损坏的内存上调用 free() */
    // if (server.daemonize && server.supervised == 0 && server.pidfile) unlink(server.pidfile);

    if (!killViaSignal) {
        // if (use_exit_on_panic)
        //     exit(1);
        abort();
    }

    /* 确保我们最后以正确的信号退出，以便在启用时转储核心文件 */
    sigemptyset (&act.sa_mask);
    act.sa_flags = SA_NODEFER | SA_ONSTACK | SA_RESETHAND;
    act.sa_handler = SIG_DFL;
    sigaction (sig, &act, NULL);
    kill(getpid(),sig);
}


/**
 * @brief Latte panic 异常处理函数
 *        记录软件失败信息、堆栈跟踪，然后终止程序
 * @param file 发生异常的文件名
 * @param line 发生异常的行号
 * @param msg  错误信息格式字符串
 * @param ...  格式化参数
 */
void _latte_panic(const char *file, int line, const char *msg, ...) {
    va_list ap;
    va_start(ap,msg);
    char fmtmsg[256];
    vsnprintf(fmtmsg,sizeof(fmtmsg),msg,ap);
    va_end(ap);

    bugReportStart();
    LATTE_LIB_LOG(LOG_WARN,"---------------------------------------------");
    LATTE_LIB_LOG(LOG_WARN,"!!! Software Failure. Press left mouse button to continue");
    LATTE_LIB_LOG(LOG_WARN,"Guru Meditation: %s #%s:%d",fmtmsg,file,line);

//     if (server.crashlog_enabled) {
// #ifdef HAVE_BACKTRACE
        logStackTrace(NULL, 1);
// #endif
//         printCrashReport();
//     }

    // 移除信号处理器以便在 abort() 时输出崩溃报告
    removeSignalHandlers();
    bugReportEnd(0, 0);
}



/**
 * @brief Latte 断言失败处理函数
 *        记录断言失败信息、堆栈跟踪，然后终止程序
 * @param estr 断言表达式字符串
 * @param file 发生断言失败的文件名
 * @param line 发生断言失败的行号
 */
void _latte_assert(const char *estr, const char *file, int line) {
    bugReportStart();
    LATTE_LIB_LOG(LOG_WARN,"=== ASSERTION FAILED ===");
    LATTE_LIB_LOG(LOG_WARN,"==> %s:%d '%s' is not true",file,line,estr);

//     if (server.crashlog_enabled) {
// #ifdef HAVE_BACKTRACE
        logStackTrace(NULL, 1);
// #endif
//         printCrashReport();
//     }

    // 移除信号处理器以便在 abort() 时输出崩溃报告
    removeSignalHandlers();
    bugReportEnd(0, 0);
}