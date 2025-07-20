
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

/* Globals */
static int bug_report_start = 0; /* True if bug report header was already logged. */
static pthread_mutex_t bug_report_start_mutex = PTHREAD_MUTEX_INITIALIZER;


void bugReportStart(void) {
    pthread_mutex_lock(&bug_report_start_mutex);
    if (bug_report_start == 0) {
        LATTE_LIB_LOG(LL_WARN|LL_RAW,
        "\n\n=== LATTE BUG REPORT START: Cut & paste starting from here ===\n");
        bug_report_start = 1;
    }
    pthread_mutex_unlock(&bug_report_start_mutex);
}

/* Return a file descriptor to write directly to the Redis log with the
 * write(2) syscall, that can be used in critical sections of the code
 * where the rest of Redis can't be trusted (for example during the memory
 * test) or when an API call requires a raw fd.
 *
 * Close it with closeDirectLogFiledes(). */
int openDirectLogFiledes(void) {
    // int log_to_stdout = logfile[0] == '\0';
    // int fd = log_to_stdout ?
    //     STDOUT_FILENO :
    //     open(logfile, O_APPEND|O_CREAT|O_WRONLY, 0644);
    // return fd;
    return STDOUT_FILENO;
}

/* Used to close what closeDirectLogFiledes() returns. */
void closeDirectLogFiledes(int fd) {
    // int log_to_stdout = logfile[0] == '\0';
    // if (!log_to_stdout) close(fd);
}

/* Logs the stack trace using the backtrace() call. This function is designed
 * to be called from signal handlers safely.
 * The eip argument is optional (can take NULL).
 * The uplevel argument indicates how many of the calling functions to skip.
 */
void logStackTrace(void *eip, int uplevel) {
    void *trace[100];
    int trace_size = 0, fd = openDirectLogFiledes();
    char *msg;
    uplevel++; /* skip this function */

    if (fd == -1) return; /* If we can't log there is anything to do. */

    /* Get the stack trace first! */
    trace_size = backtrace(trace, 100);

    msg = "\n------ STACK TRACE ------\n";
    if (write(fd,msg,strlen(msg)) == -1) {/* Avoid warning. */};

    if (eip) {
        /* Write EIP to the log file*/
        msg = "EIP:\n";
        if (write(fd,msg,strlen(msg)) == -1) {/* Avoid warning. */};
        backtrace_symbols_fd(&eip, 1, fd);
    }

    /* Write symbols to log file */
    msg = "\nBacktrace:\n";
    if (write(fd,msg,strlen(msg)) == -1) {/* Avoid warning. */};
    backtrace_symbols_fd(trace+uplevel, trace_size-uplevel, fd);

    /* Cleanup */
    closeDirectLogFiledes(fd);
}

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

void bugReportEnd(int killViaSignal, int sig) {
    struct sigaction act;

    LATTE_LIB_LOG(LL_WARN|LL_RAW,
"\n=== LATTE BUG REPORT END. Make sure to include from START to END. ===\n\n"
);

    /* free(messages); Don't call free() with possibly corrupted memory. */
    // if (server.daemonize && server.supervised == 0 && server.pidfile) unlink(server.pidfile);

    if (!killViaSignal) {
        // if (use_exit_on_panic)
        //     exit(1);
        abort();
    }

    /* Make sure we exit with the right signal at the end. So for instance
     * the core will be dumped if enabled. */
    sigemptyset (&act.sa_mask);
    act.sa_flags = SA_NODEFER | SA_ONSTACK | SA_RESETHAND;
    act.sa_handler = SIG_DFL;
    sigaction (sig, &act, NULL);
    kill(getpid(),sig);
}


void _latte_panic(const char *file, int line, const char *msg, ...) {
    va_list ap;
    va_start(ap,msg);
    char fmtmsg[256];
    vsnprintf(fmtmsg,sizeof(fmtmsg),msg,ap);
    va_end(ap);

    bugReportStart();
    LATTE_LIB_LOG(LL_WARN,"------------------------------------------------");
    LATTE_LIB_LOG(LL_WARN,"!!! Software Failure. Press left mouse button to continue");
    LATTE_LIB_LOG(LL_WARN,"Guru Meditation: %s #%s:%d",fmtmsg,file,line);

//     if (server.crashlog_enabled) {
// #ifdef HAVE_BACKTRACE
        logStackTrace(NULL, 1);
// #endif
//         printCrashReport();
//     }

    // remove the signal handler so on abort() we will output the crash report.
    removeSignalHandlers();
    bugReportEnd(0, 0);
}



void _latte_assert(const char *estr, const char *file, int line) {
    bugReportStart();
    LATTE_LIB_LOG(LL_WARN,"=== ASSERTION FAILED ===");
    LATTE_LIB_LOG(LL_WARN,"==> %s:%d '%s' is not true",file,line,estr);

//     if (server.crashlog_enabled) {
// #ifdef HAVE_BACKTRACE
        logStackTrace(NULL, 1);
// #endif
//         printCrashReport();
//     }

    // remove the signal handler so on abort() we will output the crash report.
    removeSignalHandlers();
    bugReportEnd(0, 0);
}