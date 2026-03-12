/**
 * @file ae.h
 * @brief 事件驱动框架（Async Event Loop）接口
 *        基于 epoll/kqueue/evport/io_uring/select 多路复用后端，
 *        提供文件事件（fd 读写）和定时事件（时间轮）统一管理能力。
 *
 * 主要概念：
 *   - ae_file_event_t  : 监听某个 fd 的读/写/屏障事件
 *   - ae_time_event_t  : 双向链表组织的定时回调（毫秒精度）
 *   - ae_fired_event_t : 本轮 poll 返回的就绪事件（fd + mask）
 *   - ae_event_loop_t  : 事件循环主体，持有上述所有数据结构
 */
#ifndef __LATTE_AE_H
#define __LATTE_AE_H
#include "utils/monotonic.h"
#include "log/log.h"
#include "list/list.h"
#include "func_task/func_task.h"
#include "debug/latte_debug.h"

/** @brief 操作成功返回值 */
#define AE_OK  0
/** @brief 操作失败返回值 */
#define AE_ERR -1

/** @brief 文件事件掩码：无事件注册 */
#define AE_NONE     0
/** @brief 文件事件掩码：fd 可读时触发 */
#define AE_READABLE 1
/** @brief 文件事件掩码：fd 可写时触发 */
#define AE_WRITABLE 2
/**
 * @brief 文件事件掩码：屏障标志
 *        与 AE_WRITABLE 联用时，若同一轮迭代已触发 READABLE，则不再触发 WRITABLE。
 *        适用于"先落盘再回复"的场景。
 */
#define AE_BARRIER  4

/** @brief 事件处理标志：处理文件事件 */
#define AE_FILE_EVENTS      (1<<0)
/** @brief 事件处理标志：处理定时事件 */
#define AE_TIME_EVENTS      (1<<1)
/** @brief 事件处理标志：同时处理文件和定时事件 */
#define AE_ALL_EVENTS       (AE_FILE_EVENTS|AE_TIME_EVENTS)
/** @brief 事件处理标志：不阻塞等待，立即返回 */
#define AE_DONT_WAIT        (1<<2)
/** @brief 事件处理标志：poll 前执行 beforesleep 回调 */
#define AE_CALL_BEFORE_SLEEP (1<<3)
/** @brief 事件处理标志：poll 后执行 aftersleep 回调 */
#define AE_CALL_AFTER_SLEEP  (1<<4)

/** @brief 定时事件返回值：不再重新调度（删除该定时事件） */
#define AE_NOMORE           -1
/** @brief 定时事件 ID：已被标记删除 */
#define AE_DELETED_EVENT_ID -1

/* Macros */
/** @brief 消除未使用变量警告的宏 */
#define AE_NOTUSED(V) ((void) V)

struct ae_event_loop_t;

/* Types and data structures */

/**
 * @brief 文件事件处理回调函数类型
 * @param eventLoop 事件循环指针
 * @param fd        触发事件的文件描述符
 * @param clientData 注册时绑定的用户数据
 * @param mask      触发的事件掩码（AE_READABLE / AE_WRITABLE）
 */
typedef void ae_file_proc_func(struct ae_event_loop_t *eventLoop, int fd, void *clientData, int mask);

/**
 * @brief 定时事件处理回调函数类型
 * @param eventLoop 事件循环指针
 * @param id        定时事件 ID
 * @param clientData 注册时绑定的用户数据
 * @return 返回下次触发的毫秒延迟；返回 AE_NOMORE 表示删除该定时事件
 */
typedef int ae_time_proc_func(struct ae_event_loop_t *eventLoop, long long id, void *clientData);

/**
 * @brief 定时事件析构回调函数类型（事件被删除时调用）
 * @param eventLoop 事件循环指针
 * @param clientData 注册时绑定的用户数据
 */
typedef void ae_event_finalizer_proc_func(struct ae_event_loop_t *eventLoop, void *clientData);

/**
 * @brief 睡眠前/后钩子回调函数类型
 * @param eventLoop 事件循环指针
 */
typedef void ae_before_sleep_proc_func(struct ae_event_loop_t *eventLoop);

/**
 * @brief 文件事件结构体
 *        对应一个 fd 的读/写事件注册信息
 */
typedef struct ae_file_event_t {
    int mask;                      /**< 已注册的事件掩码：AE_READABLE | AE_WRITABLE | AE_BARRIER */
    ae_file_proc_func *rfileProc;  /**< 可读事件处理回调 */
    ae_file_proc_func *wfileProc;  /**< 可写事件处理回调 */
    void *clientData;              /**< 用户自定义数据，回调时透传 */
} ae_file_event_t;

/**
 * @brief 定时事件结构体（双向链表节点）
 */
typedef struct ae_time_event_t {
    long long id;                              /**< 定时事件唯一 ID；AE_DELETED_EVENT_ID 表示待删除 */
    monotime when;                             /**< 触发时间（单调时钟微秒） */
    ae_time_proc_func *timeProc;               /**< 定时触发回调 */
    ae_event_finalizer_proc_func *finalizerProc; /**< 删除时的析构回调（可为 NULL） */
    void *clientData;                          /**< 用户自定义数据，回调时透传 */
    struct ae_time_event_t *prev;              /**< 前驱节点 */
    struct ae_time_event_t *next;              /**< 后继节点 */
    int refcount;                              /**< 引用计数，防止递归调用时被提前释放 */
} ae_time_event_t;

/**
 * @brief 已就绪文件事件（poll 返回结果）
 */
typedef struct ae_fired_event_t {
    int fd;    /**< 就绪的文件描述符 */
    int mask;  /**< 就绪的事件掩码 */
} ae_fired_event_t;

/**
 * @brief 事件循环主体结构体
 *        持有所有注册的文件事件、定时事件及多路复用后端数据
 */
typedef struct ae_event_loop_t {
    int maxfd;                          /**< 当前已注册的最大文件描述符值 */
    int setsize;                        /**< 支持监听的最大 fd 数量（events/fired 数组大小） */
    long long timeEventNextId;          /**< 下一个定时事件的自增 ID */
    ae_file_event_t *events;            /**< 文件事件数组，下标为 fd */
    ae_fired_event_t *fired;            /**< 本轮 poll 就绪事件数组 */
    ae_time_event_t *timeEventHead;     /**< 定时事件链表头 */
    int stop;                           /**< 事件循环停止标志（非 0 时 ae_main 退出） */
    void *apidata;                      /**< 多路复用后端私有数据（epoll/kqueue fd 等） */
    ae_before_sleep_proc_func *beforesleep;  /**< poll 前单函数钩子（旧接口） */
    list_t* beforesleeps;               /**< poll 前任务列表（latte_func_task_t*，新接口） */
    ae_before_sleep_proc_func *aftersleep;   /**< poll 后单函数钩子（旧接口） */
    list_t* aftersleeps;                /**< poll 后任务列表（latte_func_task_t*，新接口） */
    int flags;                          /**< 循环标志位：AE_DONT_WAIT 等 */
    void *privdata[2];                  /**< 用户私有数据槽（两个指针） */
} ae_event_loop_t;



/* ---- 文件事件 API ---- */

/**
 * @brief 注册文件事件（读/写/屏障）
 * @param eventLoop  目标事件循环
 * @param fd         要监听的文件描述符
 * @param mask       事件掩码（AE_READABLE | AE_WRITABLE | AE_BARRIER 的组合）
 * @param proc       事件触发时的回调函数
 * @param clientData 回调时透传的用户数据
 * @return AE_OK 成功；AE_ERR fd 超出范围或后端添加失败
 */
int ae_file_event_new(ae_event_loop_t *eventLoop, int fd, int mask,
        ae_file_proc_func *proc, void *clientData);

/**
 * @brief 注销文件事件
 *        从 events 数组和多路复用后端中移除指定 fd 的事件掩码
 * @param eventLoop 目标事件循环
 * @param fd        要注销的文件描述符
 * @param mask      要移除的事件掩码
 */
void ae_file_event_delete(ae_event_loop_t *eventLoop, int fd, int mask);

/**
 * @brief 获取指定 fd 当前注册的事件掩码
 * @param eventLoop 目标事件循环
 * @param fd        文件描述符
 * @return 已注册的事件掩码；fd 超出范围返回 0
 */
int ae_file_event_get_mask(ae_event_loop_t *eventLoop, int fd);

/* ---- 定时事件 API ---- */

/**
 * @brief 创建定时事件
 * @param eventLoop     目标事件循环
 * @param milliseconds  首次触发延迟（毫秒）
 * @param proc          定时触发回调（返回值决定是否重新调度）
 * @param clientData    回调时透传的用户数据
 * @param finalizerProc 事件被删除时的析构回调（可为 NULL）
 * @return 新事件 ID（>= 0）；失败返回 AE_ERR
 */
long long ae_time_event_new(ae_event_loop_t *eventLoop, long long milliseconds,
        ae_time_proc_func *proc, void *clientData,
        ae_event_finalizer_proc_func *finalizerProc);

/**
 * @brief 标记删除指定 ID 的定时事件（延迟删除，下次处理时生效）
 * @param eventLoop 目标事件循环
 * @param id        要删除的定时事件 ID
 * @return AE_OK 找到并标记；AE_ERR 未找到
 */
int ae_time_event_delete(ae_event_loop_t *eventLoop, long long id);

/**
 * @brief 处理一次事件（文件事件 + 定时事件）
 *        根据 flags 决定处理哪类事件，以及是否阻塞等待
 * @param eventLoop 目标事件循环
 * @param flags     处理标志（AE_FILE_EVENTS / AE_TIME_EVENTS / AE_DONT_WAIT 等组合）
 * @return 本次处理的事件总数
 */
int ae_process_events(ae_event_loop_t *eventLoop, int flags);

/* ---- 事件循环生命周期 API ---- */

/**
 * @brief 创建事件循环
 *        分配 events/fired 数组，初始化多路复用后端（epoll/kqueue 等）
 * @param setsize 支持监听的最大 fd 数量
 * @return 新建的事件循环指针；内存或后端初始化失败返回 NULL
 */
ae_event_loop_t *ae_event_loop_new(int setsize);

/**
 * @brief 销毁事件循环
 *        释放 events/fired 数组、所有定时事件节点、beforesleeps/aftersleeps 列表
 * @param eventLoop 要销毁的事件循环
 */
void ae_event_loop_delete(ae_event_loop_t *eventLoop);

/**
 * @brief 阻塞等待 fd 就绪（基于 poll，不进入事件循环）
 * @param fd           要等待的文件描述符
 * @param mask         期望的事件掩码（AE_READABLE | AE_WRITABLE）
 * @param milliseconds 超时时间（毫秒）
 * @return 就绪的事件掩码；超时或失败返回 0 或负值
 */
int ae_wait(int fd, int mask, long long milliseconds);

/**
 * @brief 启动事件循环主循环（阻塞直到 ae_stop 被调用）
 * @param eventLoop 目标事件循环
 */
void ae_main(ae_event_loop_t *eventLoop);

/**
 * @brief 停止事件循环（设置 stop 标志，ae_main 下一轮检测时退出）
 * @param eventLoop 目标事件循环
 */
void ae_stop(ae_event_loop_t *eventLoop);

/**
 * @brief 获取当前使用的多路复用后端名称（如 "epoll"、"kqueue"）
 * @return 后端名称字符串（静态存储，无需释放）
 */
char *ae_get_api_name(void);

/**
 * @brief 设置 poll 前单函数钩子（旧接口）
 * @param eventLoop   目标事件循环
 * @param beforesleep 钩子函数指针
 */
void ae_set_before_sleep_proc(ae_event_loop_t *eventLoop, ae_before_sleep_proc_func *beforesleep);

/**
 * @brief 向 poll 前任务列表追加一个 func_task（新接口）
 * @param eventLoop 目标事件循环
 * @param task      要追加的 latte_func_task_t 任务
 */
void ae_add_before_sleep_task(ae_event_loop_t* eventLoop, latte_func_task_t* task);

/**
 * @brief 设置 poll 后单函数钩子（旧接口）
 * @param eventLoop  目标事件循环
 * @param aftersleep 钩子函数指针
 */
void ae_set_after_sleep_proc(ae_event_loop_t *eventLoop, ae_before_sleep_proc_func *aftersleep);

/**
 * @brief 获取事件循环当前的 setsize
 * @param eventLoop 目标事件循环
 * @return 最大 fd 监听数量
 */
int ae_get_set_size(ae_event_loop_t *eventLoop);

/**
 * @brief 调整事件循环的 setsize（扩容或缩容 events/fired 数组）
 *        若已有 fd >= 新 setsize，则拒绝缩容并返回 AE_ERR
 * @param eventLoop 目标事件循环
 * @param setsize   新的最大 fd 监听数量
 * @return AE_OK 成功；AE_ERR 失败
 */
int ae_resize_set_size(ae_event_loop_t *eventLoop, int setsize);

/**
 * @brief 设置或清除 AE_DONT_WAIT 标志
 * @param eventLoop 目标事件循环
 * @param noWait    非 0 设置 DONT_WAIT（不阻塞）；0 清除（允许阻塞）
 */
void ae_set_dont_wait(ae_event_loop_t *eventLoop, int noWait);

/**
 * @brief 执行 poll 前的所有钩子（beforesleep 函数 + beforesleeps 任务列表）
 * @param eventLoop 目标事件循环
 */
void ae_do_before_sleep(ae_event_loop_t *eventLoop);

/**
 * @brief 通过事件循环后端异步读取 fd 数据
 * @param eventLoop 目标事件循环
 * @param fd        源文件描述符
 * @param buf       接收缓冲区
 * @param buf_len   缓冲区长度
 * @return 实际读取字节数；失败返回负值
 */
int ae_read(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len);

/**
 * @brief 通过事件循环后端异步写入 fd 数据
 * @param eventLoop 目标事件循环
 * @param fd        目标文件描述符
 * @param buf       待写入缓冲区
 * @param buf_len   写入字节数
 * @return 实际写入字节数；失败返回负值
 */
int ae_write(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len);

/* api 需要实现（后端接口，由各 ae_xxx.c 提供）*/
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
