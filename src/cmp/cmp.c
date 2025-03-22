#include "cmp.h"
#include <string.h>
#include <strings.h>

int str_cmp(void* a, void* b) {
    return private_str_cmp((char*)a, (char*)b);
}

int private_str_cmp(char* a, char* b) {
    return strcmp(a, b);
}

int int64_cmp(void* a, void* b) {
    return private_int64_cmp((int64_t)a, (int64_t)b);
}

int private_int64_cmp(int64_t a, int64_t b) {
    return a > b ? 1: (a < b? -1 : 0);
}



int uint64_cmp(void* a, void* b) {
    return private_uint64_cmp((uint64_t)a, (uint64_t)b);
}
int private_uint64_cmp(uint64_t a, uint64_t b) {
    return a > b ? 1: (a < b? -1 : 0);
}

int long_double_cmp(void* a, void* b) {
    return private_long_double_cmp(*(long double*)a, *(long double*)b);
}

int private_long_double_cmp(long double a, long double b) {
    return a > b ? 1: (a < b? -1 : 0);
}
