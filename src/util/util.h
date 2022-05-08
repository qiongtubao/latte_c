
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "config.h"


#ifdef LATTE_TEST
    uint32_t digits10(uint64_t v);
#endif
#define LATTE_ERR 0
#define LATTE_OK 1
int ll2string(char *dst, size_t dstlen, long long svalue);
int string2ll(const char *s, size_t slen, long long *value);
long long timeInMilliseconds(void);

