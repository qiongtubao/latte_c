#ifndef __LATTE_CMP_H
#define __LATTE_CMP_H
#include <stdlib.h>
#include <stdio.h>

typedef int cmp_func(void* a, void* b);

//int64_t cmp
int int64_cmp(void* a, void* b);
int private_int64_cmp(int64_t a, int64_t b);

#endif