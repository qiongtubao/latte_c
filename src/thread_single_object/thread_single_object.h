


#ifndef __LATTE_THREAD_SINGLE_OBJECT_H
#define __LATTE_THREAD_SINGLE_OBJECT_H

#include "dict/dict.h"
#include "sds/sds.h"
#include <pthread.h>

/**
 * 单线程对象管理器
 * 对象和注册的销毁函数 不是1对1关系
 * 
 */

typedef struct thread_single_object_manager_t {
    dict_t* dict;
    pthread_key_t latte_thread_key;
} thread_single_object_manager_t;
extern thread_single_object_manager_t* latte_thread_single_object_manager;

typedef struct thread_single_entry_t {
    void (*destroy)(void* arg);
} thread_single_entry_t;

void thread_single_object_module_init();
void thread_single_object_module_destroy();
void thread_single_object_register(char* name, void (*destroy)(void* arg));
void thread_single_object_unregister(char* name);

void* thread_single_object_get(char* name);
void* thread_single_object_set(char* name, void* value);
int thread_single_object_delete(char* name);
#endif