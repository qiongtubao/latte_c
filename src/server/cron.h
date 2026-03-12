#ifndef __LATTE_CRONS_H__
#define __LATTE_CRONS_H__

#include "vector/vector.h"

/**
 * @brief 定时任务执行函数类型
 * @param p 用户自定义参数（通常为 server 指针）
 */
typedef void (*exec_cron_func)(void* p);

/**
 * @brief 单个定时任务结构体
 * 每个任务包含执行周期和执行函数。
 */
typedef struct cron_t {
    long long period;   /**< 执行周期（单位：事件循环次数），每 period 次循环执行一次 */
    exec_cron_func fn;  /**< 定时执行的回调函数 */
} cron_t;

/**
 * @brief 创建一个定时任务
 * @param fn     任务执行回调函数
 * @param period 执行周期（事件循环次数）
 * @return cron_t* 新建任务指针，分配失败返回 NULL
 */
cron_t* cron_new(exec_cron_func fn, long long period);

/**
 * @brief 释放一个定时任务
 * @param cron 目标任务指针（可为 NULL，无操作）
 */
void cron_delete(cron_t*cron);


/**
 * @brief 定时任务管理器结构体
 * 维护一个定时任务向量，并记录总循环次数以支持周期调度。
 */
typedef struct cron_manager_t {
    long long cron_loops;   /**< 累计事件循环次数，用于判断各任务是否到达执行时机 */
    vector_t* crons;        /**< 定时任务列表（存储 cron_t* 指针） */
} cron_manager_t;

/**
 * @brief 创建定时任务管理器
 * @return cron_manager_t* 新建管理器指针，分配失败返回 NULL
 */
cron_manager_t* cron_manager_new(void);

/**
 * @brief 释放定时任务管理器及其所有任务
 * @param cron_manager 目标管理器指针（可为 NULL，无操作）
 */
void cron_manager_delete(cron_manager_t*cron_manager);

/**
 * @brief 向管理器添加定时任务
 * @param cron_manager 目标管理器
 * @param cron         要添加的任务指针
 */
void cron_manager_add_cron(cron_manager_t*cron_manager, cron_t*cron);

/**
 * @brief 从管理器移除定时任务
 * @param cron_manager 目标管理器
 * @param cron         要移除的任务指针
 */
void cron_manager_remove_cron(cron_manager_t*cron_manager, cron_t*cron);

/**
 * @brief 执行当前循环中到期的所有定时任务，并递增循环计数
 * @param cron_manager 目标管理器
 * @param arg          透传给各任务回调的参数（通常为 server 指针）
 */
void cron_manager_run(cron_manager_t*cron_manager, void* arg);

#endif