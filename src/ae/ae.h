/*
 * ae.h - 事件循环 (AE) 头文件
 * 
 * Latte C 库核心组件：高性能事件驱动框架
 * 
 * 设计特点：
 * 1. 多后端支持 (select/epoll/kqueue/iouring/evport)
 * 2. 文件事件 + 时间事件统一管理
 * 3. 支持读写分离回调
 * 4. 可扩展的钩子机制 (beforesleep/aftersleep)
 * 
 * 主要结构：
 * - ae_event_loop_t: 事件循环主结构
 * - ae_file_event_t: 文件事件 (读写)
 * - ae_time_event_t: 定时器事件
 * - ae_fired_event_t: 触发的事件队列
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef __LATTE_AE_H
#define __LATTE_AE_H

#include "utils/monotonic.h"
#include "log/log.h"
#include "list/list.h"
#include "func_task/func_task.h"
#include "debug/latte_debug.h"

/* 返回值常量 */
#define AE_OK 0        /* 操作成功 */
#define AE_ERR -1      /* 操作失败 */

/* 事件类型掩码 */
#define AE_NONE 0          /* 无事件注册 */
#define AE_READABLE 1      /* 描述符可读时触发 */
#define AE_WRITABLE 2      /* 描述符可写时触发 */
#define AE_BARRIER 4       /* 与 WRITABLE 配合使用：如果同一轮次中 READABLE 已触发，则不触发 WRITABLE。
                              用于批量持久化数据后再发送响应 */

/* 事件类别掩码 */
#define AE_FILE_EVENTS (1<<0)   /* 文件事件类别 */
#define AE_TIME_EVENTS  (1<<1)  /* 时间事件类别 */
#define AE_ALL_EVENTS   (AE_FILE_EVENTS|AE_TIME_EVENTS)  /* 所有事件 */
#define AE_DONT_WAIT    (1<<2)  /* 不等待，立即返回 */
#define AE_CALL_BEFORE_SLEEP  (1<<3)  /* 睡眠前调用回调 */
#define AE_CALL_AFTER_SLEEP   (1<<4)  /* 睡眠后调用回调 */

/* 特殊 ID 常量 */
#define AE_NOMORE -1          /* 定时器不重复触发 */
#define AE_DELETED_EVENT_ID -1  /* 已删除的事件 ID */

/* 未使用参数宏 (避免编译器警告) */
#define AE_NOTUSED(V) ((void) V)

/* 前向声明 */
struct ae_event_loop_t;

/*
 * 回调函数类型定义
 */
/* 文件事件回调函数类型
 * 参数：eventLoop - 事件循环指针
 *       fd - 触发事件的文件描述符
 *       clientData - 用户自定义数据
 *       mask - 触发的事件类型 (READABLE/WRITABLE)
 */
typedef void ae_file_proc_func(struct ae_event_loop_t *eventLoop, int fd, void *clientData, int mask);

/* 时间事件回调函数类型
 * 参数：eventLoop - 事件循环指针
 *       id - 事件 ID
 *       clientData - 用户自定义数据
 * 返回：下次触发的相对时间 (毫秒)，或 AE_NOMORE 表示不再触发
 */
typedef int ae_time_proc_func(struct ae_event_loop_t *eventLoop, long long id, void *clientData);

/* 事件清理回调函数类型
 * 参数：eventLoop - 事件循环指针
 *       clientData - 用户自定义数据
 */
typedef void ae_event_finalizer_proc_func(struct ae_event_loop_t *eventLoop, void *clientData);

/* 睡眠前回调函数类型
 * 参数：eventLoop - 事件循环指针
 */
typedef void ae_before_sleep_proc_func(struct ae_event_loop_t *eventLoop);

/*
 * 文件事件结构
 * 用于监听文件描述符的读写事件
 */
typedef struct ae_file_event_t {
    int mask;                    /* 事件掩码 (AE_READABLE|AE_WRITABLE|AE_BARRIER) */
    ae_file_proc_func *rfileProc; /* 可读事件回调函数 */
    ae_file_proc_func *wfileProc; /* 可写事件回调函数 */
    void *clientData;            /* 用户自定义数据，传递给回调函数 */
} ae_file_event_t;

/*
 * 时间事件结构
 * 用于定时器功能
 */
typedef struct ae_time_event_t {
    long long id;                    /* 事件唯一标识符 */
    monotime when;                   /* 触发时间点 (单调时钟) */
    ae_time_proc_func *timeProc;     /* 定时回调函数 */
    ae_event_finalizer_proc_func *finalizerProc;  /* 事件清理回调 */
    void *clientData;                /* 用户自定义数据 */
    struct ae_time_event_t *prev;    /* 前向链表指针 */
    struct ae_time_event_t *next;    /* 后向链表指针 */
    int refcount;                    /* 引用计数，防止递归调用中释放 */
} ae_time_event_t;

/*
 * 已触发事件结构
 * 用于存储一轮循环中触发的事件
 */
typedef struct ae_fired_event_t {
    int fd;     /* 文件描述符 */
    int mask;   /* 触发的事件类型 */
} ae_fired_event_t;

/*
 * 事件循环主结构
 * 管理所有文件和定时器事件
 */
typedef struct ae_event_loop_t {
    int maxfd;                  /* 当前注册的最大文件描述符 */
    int setsize;                /* 最大可跟踪的文件描述符数量 */
    long long timeEventNextId;  /* 下一个时间事件的 ID */
    ae_file_event_t *events;    /* 文件事件数组 (fd 为索引) */
    ae_fired_event_t *fired;    /* 已触发事件队列 */
    ae_time_event_t *timeEventHead;  /* 时间事件链表头 */
    int stop;                   /* 停止标志 */
    void *apidata;              /* 后端特定数据 (epoll/kqueue 等) */
    ae_before_sleep_proc_func *beforesleep;  /* 单例睡眠前回调 */
    list_t* beforesleeps;       /* 睡眠前回调列表 (多回调支持) */
    ae_fired_event_t *fired; /* Fired events */
    ae_time_event_t *timeEventHead;
    int stop;
    void *apidata; /* This is used for polling API specific data */
    ae_before_sleep_proc_func *beforesleep;
    list_t* beforesleeps;
    ae_before_sleep_proc_func *aftersleep;
    list_t* aftersleeps;
    int flags;
    void *privdata[2];
} ae_event_loop_t;



/* file event */
int ae_file_event_new(ae_event_loop_t *eventLoop, int fd, int mask,
        ae_file_proc_func *proc, void *clientData);
void ae_file_event_delete(ae_event_loop_t *eventLoop, int fd, int mask);
int ae_file_event_get_mask(ae_event_loop_t *eventLoop, int fd);
/* time event */
long long ae_time_event_new(ae_event_loop_t *eventLoop, long long milliseconds,
        ae_time_proc_func *proc, void *clientData,
        ae_event_finalizer_proc_func *finalizerProc);
int ae_time_event_delete(ae_event_loop_t *eventLoop, long long id);
int ae_process_events(ae_event_loop_t *eventLoop, int flags);

/* Prototypes */
ae_event_loop_t *ae_event_loop_new(int setsize);
void ae_event_loop_delete(ae_event_loop_t *eventLoop);

int ae_wait(int fd, int mask, long long milliseconds);
void ae_main(ae_event_loop_t *eventLoop);
void ae_stop(ae_event_loop_t *eventLoop);
char *ae_get_api_name(void);
void ae_set_before_sleep_proc(ae_event_loop_t *eventLoop, ae_before_sleep_proc_func *beforesleep);
void ae_add_before_sleep_task(ae_event_loop_t* eventLoop, latte_func_task_t* task);
void ae_set_after_sleep_proc(ae_event_loop_t *eventLoop, ae_before_sleep_proc_func *aftersleep);
int ae_get_set_size(ae_event_loop_t *eventLoop);
int ae_resize_set_size(ae_event_loop_t *eventLoop, int setsize);
void ae_set_dont_wait(ae_event_loop_t *eventLoop, int noWait);


void ae_do_before_sleep(ae_event_loop_t *eventLoop);
int ae_read(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len);
int ae_write(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len);

/* api 需要实现*/
// return -1 失败 0 成功
// static int ae_api_create(ae_event_loop_t *eventLoop);
// return -1 失败 0 成功
// static int ae_api_resize(ae_event_loop_t *eventLoop, int setsize);
// static void ae_api_delete(ae_event_loop_t *eventLoop);
// static int ae_api_add_event(ae_event_loop_t *eventLoop, int fd, int mask);
// static void ae_api_del_event(ae_event_loop_t *eventLoop, int fd, int delmask);
// static int ae_api_poll(ae_event_loop_t *eventLoop, struct timeval *tvp);
// static char *ae_api_name(void);
// static int ae_api_read(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len);
// static int ae_api_write(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len);
// static void ae_api_before_sleep(ae_event_loop_t *eventLoop);
// static void ae_api_after_sleep(ae_event_loop_t *eventLoop);
#endif