//
// Created by 周国栋 on 2020/6/26.
//
#include <stdio.h>
//#include "libs/MathFunctions.h"
#include "libs/sds.h"
#include "libs/zmalloc.h"
int main() {

//    double result = power(2.0,2);
//    printf("result: %f!\n", result);
//    char* v = zmalloc(5);
//    zfree(v);
    sds s = sdsnew("hello");
    printf("result: %s!\n", s);
    sdsfree(s);
    printf("lib: %s\n", ZMALLOC_LIB);
    return 0;
}


