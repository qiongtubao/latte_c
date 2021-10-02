
#ifndef __BIO_H
#define __BIO_H
#include <signal.h>
#include <errno.h>
#include "latte_c.h"
void bioInit(void);
void* bioProcessBackgroundJobs(void *arg);

/* Background job opcodes */
#define BIO_CLOSE_FILE    0 /* Deferred close(2) syscall. */
#define BIO_AOF_FSYNC     1 /* Deferred AOF fsync. */
#define BIO_LAZY_FREE     2 /* Deferred objects freeing. */

#endif