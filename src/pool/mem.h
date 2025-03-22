#ifndef LATTE_C_POOL_H
#define LATTE_C_POOL_H

#include "mutex/mutex.h"
#include "sds/sds.h"
#include "set/set.h"
#include "list/list.h"

typedef struct mem_pool_t  {
    latte_mutex_t* mutex;
    sds name;
    //是否可扩容
    bool dynamic;
    //总个数
    int size;
    //一个指针对象大小
    int item_size;
    //1个pool 包含多少item个数
    int item_num_per_pool;

    //所有pool内存管理
    list_t* pools;
    //已经使用的对象管理
    set_t* used;
    //空闲对象管理
    list_t* frees;
} mem_pool_t;

mem_pool_t* mem_pool_new(sds name);
int mem_pool_delete(mem_pool_t* pool);
//扩容
void mem_pool_clean_up(mem_pool_t* mem_pool);
int mem_pool_extend(mem_pool_t* pool);
void* mem_pool_alloc(mem_pool_t* mem_pool);
void mem_pool_free(mem_pool_t* mem_pool, void* result);
int mem_pool_init(mem_pool_t* mem_pool, int item_size, bool dynamic, int pool_num, int item_num_per_pool);
#endif