
#include "latte_debug.h"
#include <fcntl.h>
#include <unistd.h>
#include <execinfo.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

/* Globals */
static int bug_report_start = 0; /* True if bug report header was already logged. */
static pthread_mutex_t bug_report_start_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Optional sink installed by the embedder. NULL means "write to a raw fd". */
static latte_debug_sink debug_sink = NULL;

/* Destination file for crash output. Empty means stdout. Fixed size so that
 * setting it never allocates and reading it from a crash path is safe. */
#define LATTE_DEBUG_LOGFILE_MAX 1024
static char debug_logfile[LATTE_DEBUG_LOGFILE_MAX] = "";

void latte_debug_set_sink(latte_debug_sink fn) {
    debug_sink = fn;
}

void latte_debug_set_logfile(const char *path) {
    if (path == NULL) {
        debug_logfile[0] = '\0';
        return;
    }
    size_t len = strlen(path);
    if (len >= sizeof(debug_logfile)) len = sizeof(debug_logfile) - 1;
    memcpy(debug_logfile, path, len);
    debug_logfile[len] = '\0';
}

/* Return a file descriptor to write crash output directly with the write(2)
 * syscall. Used in critical sections where the rest of the process can't be
 * trusted (for example after a failed assertion) or when an API call requires a
 * raw fd.
 *
 * Close it with closeDirectLogFiledes(). */
static int openDirectLogFiledes(void) {
    int log_to_stdout = debug_logfile[0] == '\0';
    return log_to_stdout ?
        STDOUT_FILENO :
        open(debug_logfile, O_APPEND|O_CREAT|O_WRONLY, 0644);
}

/* Used to close what openDirectLogFiledes() returns. */
static void closeDirectLogFiledes(int fd) {
    if (debug_logfile[0] != '\0' && fd != -1) close(fd);
}

/* Write raw bytes to the crash output. No allocation, no locking. */
static void debug_write(const char *msg, size_t len) {
    if (debug_sink) {
        debug_sink(msg, len);
        return;
    }
    int fd = openDirectLogFiledes();
    if (fd == -1) return;
    if (write(fd, msg, len) == -1) {/* Avoid warning. */};
    closeDirectLogFiledes(fd);
}

/* printf-style crash output. Formats into a stack buffer (vsnprintf is
 * async-signal-unsafe in theory but does not allocate here) and appends a
 * newline, mirroring what the log module would have produced. */
static void debug_logf(const char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));

static void debug_logf(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n > sizeof(buf) - 2) n = sizeof(buf) - 2;
    buf[n++] = '\n';
    debug_write(buf, (size_t)n);
}

static void bugReportStart(void) {
    pthread_mutex_lock(&bug_report_start_mutex);
    if (bug_report_start == 0) {
        debug_logf("\n\n=== LATTE BUG REPORT START: Cut & paste starting from here ===\n");
        bug_report_start = 1;
    }
    pthread_mutex_unlock(&bug_report_start_mutex);
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

static void removeSignalHandlers(void) {
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

static void bugReportEnd(int killViaSignal, int sig) {
    struct sigaction act;

    debug_logf("\n=== LATTE BUG REPORT END. Make sure to include from START to END. ===\n");

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
    debug_logf("---------------------------------------------");
    debug_logf("!!! Software Failure. Press left mouse button to continue");
    debug_logf("Guru Meditation: %s #%s:%d",fmtmsg,file,line);

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
    debug_logf("=== ASSERTION FAILED ===");
    debug_logf("==> %s:%d '%s' is not true",file,line,estr);

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
