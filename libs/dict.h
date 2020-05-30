//
// Created by 周国栋 on 2020/5/27.
//

#ifndef LATTE_C_DICT_H
#define LATTE_C_DICT_H
typedef struct dictEntry {
    void *key;//8
    union {
      void *val;
      uint64_t u64;
      int64_t s64;
      double d;
    } v;
    struct dictEntry *next;//8
} dictEntry;

#endif //LATTE_C_DICT_H
