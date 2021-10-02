#include "latteassert.h"
#include <stdio.h>
#include <stdarg.h>
void _latteAssert( char *estr,  char *file, int line) {
    printf("=== ASSERTION FAILED ===\n");
    printf("==> %s:%d '%s' is not true\n",file,line,estr);
#ifdef HAVE_BACKTRACE
    server.assert_failed = estr;
    server.assert_file = file;
    server.assert_line = line;
    printf("(forcing SIGSEGV to print the bug report.)\n");
#endif
    *((char*)-1) = 'x';
}


void _lattePanic(const char *file, int line, const char *msg, ...) {
    va_list ap;
    va_start(ap,msg);
    char fmtmsg[256];
    vsnprintf(fmtmsg,sizeof(fmtmsg),msg,ap);
    va_end(ap);

    // bugReportStart();
    printf("------------------------------------------------");
    printf("!!! Software Failure. Press left mouse button to continue");
    printf("Guru Meditation: %s #%s:%d",fmtmsg,file,line);
#ifdef HAVE_BACKTRACE
    printf("(forcing SIGSEGV in order to print the stack trace)");
#endif
    printf("------------------------------------------------");
    *((char*)-1) = 'x';
}