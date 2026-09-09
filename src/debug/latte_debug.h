#ifndef __LATTE_DEBUG_H
#define __LATTE_DEBUG_H

#include <stddef.h>

/* This module is intentionally a dependency-free leaf: it must stay usable from
 * the lowest layers of the library (and from a process whose heap may already be
 * corrupted), so it never allocates and never calls into the log module.
 *
 * Crash output is written with raw write(2) to a file descriptor. By default that
 * is stdout; call latte_debug_set_logfile() to redirect it, or install a custom
 * sink with latte_debug_set_sink() to route the bytes somewhere else. */

/* Receives already formatted crash output. Must not allocate. */
typedef void (*latte_debug_sink)(const char *msg, size_t len);

/* Install a custom sink, or NULL to restore the default write(2) behaviour. */
void latte_debug_set_sink(latte_debug_sink fn);

/* Append crash output to `path` instead of stdout. The path is copied into a
 * fixed size static buffer (no allocation). NULL or "" restores stdout. */
void latte_debug_set_logfile(const char *path);

/* Debugging stuff */
void _latte_assert(const char *estr, const char *file, int line);
#ifdef __GNUC__
void _latte_panic(const char *file, int line, const char *msg, ...)
    __attribute__ ((format (printf, 3, 4)));
#else
void _latte_panic(const char *file, int line, const char *msg, ...);
#endif

/* Dump the current backtrace to the crash output. Safe to call from a signal
 * handler. `eip` is optional, `uplevel` is how many callers to skip. */
void logStackTrace(void *eip, int uplevel);

#if __GNUC__ >= 4
#define latte_unreachable __builtin_unreachable
#else
#define latte_unreachable abort
#endif

#define latte_assert(_e) ((_e)?(void)0 : (_latte_assert(#_e,__FILE__,__LINE__),latte_unreachable()))
#define latte_panic(...) _latte_panic(__FILE__,__LINE__,__VA_ARGS__),latte_unreachable()


#endif
