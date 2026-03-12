/**
 * @file coroutine.h
 * @brief 协程模块接口 - 类似 Go 的 goroutine
 *
 * 并发：
 *   单线程内多协程协作式并发。同一时刻只有一个协程在运行，通过 latte_yield() 主动让出后，
 *   调度器切换到其他就绪协程，从而在逻辑上多任务"并发"执行（如多个 sleep 可重叠，总耗时约最长者）。
 *
 * 等待：
 *   WaitGroup 提供"等 N 个子协程结束"的语义：主协程 Add(n) 后启动 n 个子协程，每个子协程结束时
 *   Done()，主协程 Wait() 会阻塞（不断 yield）直到计数归零，再继续执行。
 *
 * 用法：在任务函数里调用 latte_yield() 主动让出，需要时用 latte_go(fn, arg) 启动新协程。
 * 特性：协作式调度、每协程独立栈（默认 256KB）。
 * 依赖：POSIX ucontext，macOS / Linux 可用。
 */
#ifndef __LATTE_COROUTINE_H__
#define __LATTE_COROUTINE_H__

#include <stddef.h>

/**
 * @brief 协程入口函数类型
 * @param arg 用户自定义参数，由 latte_go 传入
 */
typedef void (*latte_coroutine_fn)(void* arg);

/**
 * @brief 启动一个新协程（类似 go fn(arg)）
 *        创建后立即参与调度；若当前有协程在运行，会先让出再运行新协程。
 *        函数返回时，该协程已运行到首次 yield 或结束。
 * @param fn  协程入口函数
 * @param arg 透传给 fn 的用户参数
 */
void latte_go(latte_coroutine_fn fn, void* arg);

/**
 * @brief 主动让出 CPU，切换到其他就绪协程
 *        仅可在协程入口函数 fn 内部调用；
 *        调度器会在下次轮到该协程时从此处恢复执行。
 */
void latte_yield(void);

/**
 * @brief 设置每个协程的栈大小（字节）
 *        须在首次调用 latte_go 之前调用，默认 256 * 1024 字节。
 * @param size 栈大小（字节）
 */
void latte_coroutine_set_stack_size(size_t size);

/**
 * @brief WaitGroup：协程级并发等待（类似 Go sync.WaitGroup）
 *
 * 等待语义：调用方协程 Wait() 后不再占用 CPU，通过反复 yield 让其他协程运行；
 * 当所有子协程都调用了 Done()、计数变为 0 时，调度器再次调度到该协程，Wait() 返回。
 */
typedef struct latte_waitgroup_t latte_waitgroup_t;

/**
 * @brief 创建 WaitGroup 实例
 *        用完须调用 latte_waitgroup_free 释放
 * @return 新建的 WaitGroup 指针
 */
latte_waitgroup_t* latte_waitgroup_create(void);

/**
 * @brief 释放 WaitGroup 实例
 * @param wg 要释放的 WaitGroup 指针
 */
void latte_waitgroup_free(latte_waitgroup_t* wg);

/**
 * @brief 计数器加 delta（可为负）
 *        通常在启动子协程前调用 Add(n) 注册 n 个待等待协程
 * @param wg    目标 WaitGroup
 * @param delta 计数增量（正数增加，负数减少）
 */
void latte_waitgroup_add(latte_waitgroup_t* wg, int delta);

/**
 * @brief 计数器减 1（等价于 Add(-1)）
 *        子协程完成时调用，表示"我完成了"
 * @param wg 目标 WaitGroup
 */
void latte_waitgroup_done(latte_waitgroup_t* wg);

/**
 * @brief 等待计数器归零（阻塞当前协程）
 *        内部通过反复 yield 让出 CPU，仅可在协程内调用；
 *        当计数器变为 0 时，调度器将重新调度到该协程，函数返回。
 * @param wg 目标 WaitGroup
 */
void latte_waitgroup_wait(latte_waitgroup_t* wg);

#endif /* __LATTE_COROUTINE_H__ */
