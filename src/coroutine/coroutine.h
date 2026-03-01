/**
 * 协程模块 - 类似 Go 的 goroutine
 *
 * 并发：
 *   单线程内多协程协作式并发。同一时刻只有一个协程在运行，通过 latte_yield() 主动让出后，
 *   调度器切换到其他就绪协程，从而在逻辑上多任务“并发”执行（如多个 sleep 可重叠，总耗时约最长者）。
 *
 * 等待：
 *   WaitGroup 提供“等 N 个子协程结束”的语义：主协程 Add(n) 后启动 n 个子协程，每个子协程结束时
 *   Done()，主协程 Wait() 会阻塞（不断 yield）直到计数归零，再继续执行。
 *
 * 用法：在任务函数里调用 latte_yield() 主动让出，需要时用 latte_go(fn, arg) 启动新协程。
 * 特性：协作式调度、每协程独立栈(默认256KB)。
 * 依赖：POSIX ucontext，macOS / Linux 可用。
 */
#ifndef __LATTE_COROUTINE_H__
#define __LATTE_COROUTINE_H__

#include <stddef.h>

/* 协程入口函数类型：void fn(void* arg) */
typedef void (*latte_coroutine_fn)(void* arg);

/**
 * 启动一个协程（类似 go fn(arg)）
 * 创建后立即参与调度；若当前有协程在运行，会先让出再运行新协程。
 * 函数返回时，该协程已运行到首次 yield 或结束。
 */
void latte_go(latte_coroutine_fn fn, void* arg);

/**
 * 主动让出 CPU，切换到其他就绪协程。
 * 仅可在协程入口函数 fn 内部调用。
 */
void latte_yield(void);

/**
 * 设置每个协程的栈大小（字节）。
 * 须在首次调用 latte_go 之前调用，默认 256*1024。
 */
void latte_coroutine_set_stack_size(size_t size);

/* ---------- WaitGroup：并发等待（类似 Go sync.WaitGroup） ----------
 * 等待语义：调用方协程 Wait() 后不再占用 CPU，通过反复 yield 让其他协程运行；
 * 当所有子协程都调用了 Done()、计数变为 0 时，调度器再次调度到该协程，Wait() 返回。
 */
typedef struct latte_waitgroup_t latte_waitgroup_t;

/** 创建 WaitGroup，用完须 latte_waitgroup_free */
latte_waitgroup_t* latte_waitgroup_create(void);
void latte_waitgroup_free(latte_waitgroup_t* wg);

/** 计数器加 delta（可为负），通常在启动子协程前 Add(n) */
void latte_waitgroup_add(latte_waitgroup_t* wg, int delta);
/** 等价于 Add(-1)，子协程结束时调用，表示“我完成了” */
void latte_waitgroup_done(latte_waitgroup_t* wg);
/** 等待：当前协程阻塞直到计数器为 0（内部通过 yield 让出，仅可在协程内调用） */
void latte_waitgroup_wait(latte_waitgroup_t* wg);

#endif /* __LATTE_COROUTINE_H__ */
