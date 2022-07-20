#include <stdint.h>

/* Reverse the bytes in a 64-bit word. */
static inline uint64_t rev8(uint64_t a) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(a);
#else
    uint64_t m;

    m = UINT64_C(0xff00ff00ff00ff);
    a = ((a >> 8) & m) | (a & m) << 8;
    m = UINT64_C(0xffff0000ffff);
    a = ((a >> 16) & m) | (a & m) << 16;
    return a >> 32 | a << 32;
#endif
}