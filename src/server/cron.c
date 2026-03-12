/**
 * @file cron.c
 * @brief 定时任务管理器实现文件
 * @details 实现了定时任务的创建、管理和执行功能
 * @author latte_c
 * @date 2026-03-12
 */

#include "cron.h"
#include "zmalloc/zmalloc.h"

/**
 * @brief 创建一个定时任务
 * @param fn     任务执行回调函数
 * @param period 执行周期（事件循环次数），每隔 period 次触发一次
 * @return cron_t* 新建任务指针，分配失败返回 NULL
 */
cron_t* cron_new(exec_cron_func fn, long long period) {
    cron_t* cron = (cron_t*)zmalloc(sizeof(cron_t));
    if (cron == NULL) {
        return NULL;
    }
    cron->period = period;
    cron->fn = fn;
    return cron;
}

/**
 * @brief 释放一个定时任务结构体
 * @param cron 目标任务指针（为 NULL 时直接返回）
 */
void cron_delete(cron_t*cron) {
    if (cron == NULL) {
        return;
    }
    zfree(cron);
}

/**
 * @brief 创建定时任务管理器，初始化循环计数器和任务向量
 * @return cron_manager_t* 新建管理器指针，分配失败返回 NULL
 */
cron_manager_t* cron_manager_new(void) {
    cron_manager_t* cron_manager = (cron_manager_t*)zmalloc(sizeof(cron_manager_t));
    if (cron_manager == NULL) {
        return NULL;
    }
    cron_manager->cron_loops = 0;
    cron_manager->crons = vector_new(); /* 创建存储 cron_t* 的动态数组 */
    if (cron_manager->crons == NULL) {
        zfree(cron_manager);
        return NULL;
    }
    return cron_manager;
}

/**
 * @brief 释放定时任务管理器及其持有的所有任务
 * 遍历 crons 向量，依次调用 cron_delete 释放每个任务，
 * 再删除向量，最后释放管理器本身。
 * @param cron_manager 目标管理器指针（为 NULL 时直接返回）
 */
void cron_manager_delete(cron_manager_t*cron_manager) {
    if (cron_manager == NULL) {
        return;
    }
    /* 遍历所有任务，逐一释放 */
    for (int i = 0; i < vector_size(cron_manager->crons); i++) {
        cron_t* cron = (cron_t*)vector_get(cron_manager->crons, i);
        cron_delete(cron);
    }
    vector_delete(cron_manager->crons);
    zfree(cron_manager);
}

/**
 * @brief 向管理器追加一个定时任务
 * @param cron_manager 目标管理器指针（为 NULL 时直接返回）
 * @param cron         要添加的任务指针
 */
void cron_manager_add_cron(cron_manager_t* cron_manager, cron_t*cron) {
    if (cron_manager == NULL) {
        return;
    }
    vector_add(cron_manager->crons, cron);
}

/**
 * @brief 从管理器中移除指定定时任务（不释放任务本身）
 * @param cron_manager 目标管理器指针（为 NULL 时直接返回）
 * @param cron         要移除的任务指针
 */
void cron_manager_remove_cron(cron_manager_t*cron_manager, cron_t*cron) {
    if (cron_manager == NULL) {
        return;
    }
    vector_remove(cron_manager->crons, cron);
}

/**
 * @brief 执行本次循环中所有到期任务，并将 cron_loops 加一
 *
 * 遍历所有任务，若 cron_loops % period == 0 则执行该任务的回调。
 * 跳过 fn 为 NULL 的无效任务。
 * @param cron_manager 目标管理器指针（为 NULL 时直接返回）
 * @param arg          透传给各任务回调的参数（通常为 server 指针）
 */
void cron_manager_run(cron_manager_t*cron_manager, void* arg) {
    if (cron_manager == NULL) {
        return;
    }
    vector_t* crons = cron_manager->crons;
    for (int i = 0; i < vector_size(crons); i++) {
        cron_t* cron = (cron_t*)vector_get(crons, i);
        if (cron == NULL || cron->fn == NULL) {
            continue; /* 跳过无效任务 */
        }
        /* 按周期触发：当循环次数是周期的整数倍时执行 */
        if (cron_manager->cron_loops % cron->period == 0) {
            cron->fn(arg);
        }
    }
    cron_manager->cron_loops++; /* 每次调用后递增循环计数 */
}
