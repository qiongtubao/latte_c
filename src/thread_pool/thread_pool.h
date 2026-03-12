

#ifndef __LATTE_THREAD_POOL_H
#define __LATTE_THREAD_POOL_H

#include <pthread.h>
#include "vector/vector.h"
#include "utils/atomic.h"
#include "time/localtime.h"
#include "sds/sds.h"

/** @brief 线程本地存储的线程 ID，-1 表示主线程 */
static __thread int thread_id = -1;

/**
 * @brief 判断当前线程是否为主线程（thread_id == -1）
 * @return bool 是主线程返回 true，否则返回 false
 */
bool is_main_thread();

/**
 * @brief 获取当前线程的 ID
 * @return int 当前线程 ID，主线程返回 -1
 */
int  get_thread_id();

/**
 * @brief 设置当前线程的 ID
 * @param tid 线程 ID
 */
void set_thread_id(int tid);


/** @brief 线程运行状态 */
#define LATTE_THREAD_RUNNING 0
/** @brief 线程已停止状态 */
#define LATTE_THREAD_STOP 1

/**
 * @brief 单个线程的描述结构体
 */
typedef struct latte_thread_t {
    int tid;                        /**< 线程在线程池中的序号 */
    pthread_t thread;               /**< POSIX 线程句柄 */
    sds_t name;                     /**< 线程名称（SDS 字符串） */
    void* (*run)(void* ctx);        /**< 线程主函数 */
    int status;                     /**< 线程状态（RUNNING/STOP） */
    void* ctx;                      /**< 线程上下文数据指针 */
    pthread_t thread_id;            /**< 线程 ID（同 thread，兼容用） */
    pthread_mutex_t mutex;          /**< 线程互斥锁 */
} latte_thread_t;

/**
 * @brief 线程池结构体
 *
 * 管理一组工作线程，支持动态扩缩容（栈式：尾部添加/删除）。
 */
typedef struct thread_pool_t {
    vector_t* threads;                              /**< 线程指针向量（栈式，尾部扩缩） */
    latte_thread_t* (*create_thread_func)();        /**< 工厂函数：创建新线程 */
    void (*delete_thread_func)(latte_thread_t* thread); /**< 析构函数：销毁线程 */
    size_t max_thread_num;                          /**< 最大线程数量上限 */
    size_t min_thread_num;                          /**< 最小线程数量（初始化时创建） */
    char* cpu_list;                                 /**< CPU 亲和性绑定列表字符串 */
} thread_pool_t;

/**
 * @brief 创建线程池并启动 min_thread_num 个工作线程
 * @param min_thread_num      最小（初始）线程数
 * @param max_thread_num      最大线程数
 * @param create_thread_func  线程创建工厂函数（传入 tid）
 * @param delete_thread_func  线程销毁函数
 * @return thread_pool_t* 新建线程池指针
 */
thread_pool_t* latte_thread_pool_new(size_t min_thread_num,
    size_t max_thread_num,
    latte_thread_t* (*create_thread_func)(int tid),
    void (*delete_thread_func)(latte_thread_t* thread)
);

/**
 * @brief 向线程池追加一个新工作线程（不超过 max_thread_num）
 * @param pool 目标线程池
 * @return bool 成功返回 true，池已满或创建失败返回 false
 */
bool latte_thread_pool_add_thread(thread_pool_t* pool);

/**
 * @brief 从线程池移除最后一个工作线程（栈式缩容）
 * @param pool 目标线程池
 * @return bool 始终返回 true
 */
bool latte_thread_pool_delete_thread(thread_pool_t* pool);

/**
 * @brief 销毁线程池：依次删除所有线程，释放向量和池本身
 * @param pool 目标线程池
 */
void latte_thread_pool_delete(thread_pool_t* pool);

/* ==================== 工具函数 ==================== */

/**
 * @brief 设置当前线程的 CPU 亲和性（仅在编译时启用 USE_SETCPUAFFINITY 时有效）
 * @param cpulist CPU 核心列表字符串（如 "0,1,2"）
 */
void setcpuaffinity(const char *cpulist);

/**
 * @brief 设置当前线程的 CPU 亲和性（封装 setcpuaffinity）
 * @param cpu_list CPU 核心列表字符串
 */
void latte_set_cpu_affinity(const char* cpu_list);

/**
 * @brief 将当前线程设置为可被异步 cancel 的状态
 * 启用 PTHREAD_CANCEL_ENABLE 和 PTHREAD_CANCEL_ASYNCHRONOUS，
 * 允许 pthread_cancel 在任意时刻立即终止线程。
 */
void latte_make_thread_killable();



#endif
