#ifndef __LATTE_CMP_H
#define __LATTE_CMP_H
#include <stdlib.h>
#include <stdio.h>

typedef int cmp_func(void* a, void* b);

// str cmp
int str_cmp(void* a, void* b);
int private_str_cmp(char* a, char* b);

//int64_t cmp
int int64_cmp(void* a, void* b);
int private_int64_cmp(int64_t a, int64_t b);

//uint64_cmp
int uint64_cmp(void* a, void* b);
int private_uint64_cmp(uint64_t a, uint64_t b);

/**
 * 由于long double字节数为16 无法转化成void*  
 * 所以这里通用比较需要对比的是long double* 
 * */
int long_double_cmp(void* a, void* b);
int private_long_double_cmp(long double a, long double b);
#endif