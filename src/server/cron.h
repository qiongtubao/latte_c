/*
 * cron.h - server 模块头文件
 * 
 * Latte C 库组件
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef __LATTE_CRONS_H__
#define __LATTE_CRONS_H__

#include "vector/vector.h"

typedef void (*exec_cron_func)(void* p);

typedef struct cron_t {
    long long period;
    exec_cron_func fn;
} cron_t;

cron_t* cron_new(exec_cron_func fn, long long period);
void cron_delete(cron_t*cron);


typedef struct cron_manager_t {
    long long cron_loops;
    vector_t* crons;
} cron_manager_t;

cron_manager_t* cron_manager_new(void);
void cron_manager_delete(cron_manager_t*cron_manager);

void cron_manager_add_cron(cron_manager_t*cron_manager, cron_t*cron);
void cron_manager_remove_cron(cron_manager_t*cron_manager, cron_t*cron);
void cron_manager_run(cron_manager_t*cron_manager, void* arg);

#endif