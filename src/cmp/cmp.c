#include "cmp.h"

int int64_cmp(void* a, void* b) {
    return private_int64_cmp((int64_t)a, (int64_t)b);
}

int private_int64_cmp(int64_t a, int64_t b) {
    return a > b ? 1: (a < b? -1 : 0);
}