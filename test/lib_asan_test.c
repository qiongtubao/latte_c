
#include <stdio.h>
#include "zmalloc/zmalloc.h"

void *test_zmalloc(size_t size) {
    return zmalloc(size);
}

int main(int argc, char *argv[]) {
    void *p = test_zmalloc(100);
    printf("p: %p\n", p);
    return 1;
}