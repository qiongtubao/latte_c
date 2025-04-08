#ifndef __LATTE_DEBUG_H
#define __LATTE_DEBUG_H



/* Debugging stuff */
void _latte_assert(const char *estr, const char *file, int line);
#ifdef __GNUC__
void _latte_panic(const char *file, int line, const char *msg, ...)
    __attribute__ ((format (printf, 3, 4)));
#else
void _latte_panic(const char *file, int line, const char *msg, ...);
#endif

#if __GNUC__ >= 4
#define latte_unreachable __builtin_unreachable
#else
#define latte_unreachable abort
#endif

#define latte_assert(_e) ((_e)?(void)0 : (_latte_assert(#_e,__FILE__,__LINE__),latte_unreachable()))
#define latte_panic(...) _latte_panic(__FILE__,__LINE__,__VA_ARGS__),latte_unreachable()


#endif