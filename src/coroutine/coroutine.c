/**
 * @file coroutine.c
 * @brief 协程实现模块，基于POSIX ucontext提供协作式多任务调度
 */

/**
 * 协程实现：基于 POSIX ucontext，每个协程独立栈，协作式调度。
 *
 * 并发：就绪队列 + 单调度循环。run_scheduler 循环 pop 协程、setcontext 执行；
 * 协程通过 latte_yield() 或返回（DONE）时 swapcontext 回到调度器，调度器再调度下一个，
 * 从而实现多协程在单线程内交替执行（并发感）。
 *
 * 等待：WaitGroup 的 Wait() 即 while (count > 0) latte_yield()，不占 CPU，
 * 让其他协程运行直至 count 被 Done() 减为 0。
 *
 * macOS 上 ucontext 已弃用但仍可用，此处屏蔽弃用警告。
 * Linux glibc 需 _GNU_SOURCE 才能暴露 ucontext_t 的 uc_stack/uc_link 成员。
 */
#define _XOPEN_SOURCE 600
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#if defined(__APPLE__)
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "coroutine.h"
#include "zmalloc/zmalloc.h"

#if defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 600
#include <ucontext.h>
#else
#error "coroutine requires ucontext: define _XOPEN_SOURCE>=600 (e.g. -D_XOPEN_SOURCE=600)"
#endif
#include <string.h>
#include <stddef.h>

/** 协程状态枚举 */
enum {
    LATTE_CORO_READY,  /**< 就绪状态 */
    LATTE_CORO_RUNNING, /**< 运行状态 */
    LATTE_CORO_DONE     /**< 完成状态 */
};

/**
 * @brief 协程结构体
 */
typedef struct latte_coroutine_t {
    ucontext_t ctx;                    /**< 协程上下文 */
    void* stack;                       /**< 协程栈指针 */
    int state;                         /**< 协程状态 */
    latte_coroutine_fn fn;             /**< 协程函数指针 */
    void* arg;                         /**< 协程函数参数 */
    struct latte_coroutine_t* next;    /**< 下一个协程指针，用于队列链表 */
} latte_coroutine_t;

/* 默认栈大小 256KB */
#define LATTE_CORO_DEFAULT_STACK_SIZE (256 * 1024)
static size_t s_stack_size = LATTE_CORO_DEFAULT_STACK_SIZE;  /**< 协程栈大小 */

static ucontext_t s_scheduler_ctx;         /**< 调度器上下文 */
static latte_coroutine_t* s_ready_head = NULL;  /**< 就绪队列头，并发调度即从此队列轮转取协程 */
static latte_coroutine_t* s_ready_tail = NULL;  /**< 就绪队列尾 */
static latte_coroutine_t* s_current = NULL;     /**< 当前正在运行的协程 */
/* 协程内调用 latte_go 时不在协程栈上 getcontext，由调度器创建新协程 */
static latte_coroutine_fn s_pending_fn = NULL;  /**< 等待创建的协程函数 */
static void* s_pending_arg = NULL;              /**< 等待创建的协程参数 */

/**
 * @brief 将协程加入就绪队列尾部
 * @param c 要加入的协程指针
 */
static void push_ready(latte_coroutine_t* c) {
    c->next = NULL;
    if (s_ready_tail)
        s_ready_tail->next = c;
    else
        s_ready_head = c;
    s_ready_tail = c;
}

/**
 * @brief 从就绪队列头部取出一个协程
 * @return 协程指针，队列为空时返回NULL
 */
static latte_coroutine_t* pop_ready(void) {
    latte_coroutine_t* c = s_ready_head;
    if (!c) return NULL;
    s_ready_head = c->next;
    if (s_ready_head == NULL) s_ready_tail = NULL;
    c->next = NULL;
    return c;
}

static void coro_wrapper(void);

/**
 * @brief 调度循环：驱动并发。每次从就绪队列取一个协程执行，该协程 yield 或结束时回到此处，
 * 再取下一个，直到队列空。所有"等待"最终都通过 yield 把 CPU 让给其他协程。
 */
static void run_scheduler(void) {
    for (;;) {
        /* 统一返回点：协程 yield/完成时 swapcontext 回到此处 */
        getcontext(&s_scheduler_ctx);
        if (s_current != NULL) {
            if (s_current->state == LATTE_CORO_READY)
                push_ready(s_current);
            else if (s_current->state == LATTE_CORO_DONE) {
                // 清理完成的协程
                latte_coroutine_t* done = s_current;
                s_current = NULL;
                if (done->stack) {
                    zfree(done->stack);
                    done->stack = NULL;
                }
                zfree(done);
            } else
                s_current = NULL;
        }
        /* 协程内 latte_go 时把创建请求留到调度器里做，此处 getcontext 才正确 */
        while (s_pending_fn != NULL) {
            // 创建新协程
            latte_coroutine_t* c = (latte_coroutine_t*)zmalloc(sizeof(latte_coroutine_t));
            memset(c, 0, sizeof(*c));
            c->fn = s_pending_fn;
            c->arg = s_pending_arg;
            s_pending_fn = NULL;
            s_pending_arg = NULL;
            c->state = LATTE_CORO_READY;
            c->stack = zmalloc(s_stack_size);
            if (c->stack) {
                // 初始化协程上下文
                getcontext(&c->ctx);
                c->ctx.uc_stack.ss_sp = c->stack;
                c->ctx.uc_stack.ss_size = s_stack_size;
                c->ctx.uc_stack.ss_flags = 0;
                c->ctx.uc_link = &s_scheduler_ctx;
                makecontext(&c->ctx, (void (*)(void))coro_wrapper, 0);
                push_ready(c);
            } else {
                zfree(c);
            }
        }
        // 所有协程都执行完毕，退出调度循环
        if (s_ready_head == NULL)
            break;
        // 切换到下一个协程执行
        latte_coroutine_t* c = pop_ready();
        s_current = c;
        c->state = LATTE_CORO_RUNNING;
        setcontext(&c->ctx);
    }
}

/**
 * @brief 启动新协程
 * @param fn 协程函数指针
 * @param arg 协程函数参数
 */
void latte_go(latte_coroutine_fn fn, void* arg) {
    if (s_current != NULL) {
        /* 在协程内调用：不在此处 getcontext，交给调度器创建，避免覆盖当前协程 ctx */
        s_pending_fn = fn;
        s_pending_arg = arg;
        push_ready(s_current);
        swapcontext(&s_current->ctx, &s_scheduler_ctx);
        return;
    }
    // 主线程创建协程
    latte_coroutine_t* c = (latte_coroutine_t*)zmalloc(sizeof(latte_coroutine_t));
    memset(c, 0, sizeof(*c));
    c->fn = fn;
    c->arg = arg;
    c->state = LATTE_CORO_READY;
    c->stack = zmalloc(s_stack_size);
    if (!c->stack) {
        zfree(c);
        return;
    }
    getcontext(&c->ctx);
    c->ctx.uc_stack.ss_sp = c->stack;
    c->ctx.uc_stack.ss_size = s_stack_size;
    c->ctx.uc_stack.ss_flags = 0;
    c->ctx.uc_link = &s_scheduler_ctx;
    makecontext(&c->ctx, (void (*)(void))coro_wrapper, 0);
    push_ready(c);
    run_scheduler();
}

/**
 * @brief 协程包装函数，负责调用用户函数并处理协程完成逻辑
 */
static void coro_wrapper(void) {
    latte_coroutine_t* c = s_current;
    if (c && c->fn)
        c->fn(c->arg);
    if (c) {
        c->state = LATTE_CORO_DONE;
        /* 用 swapcontext 回到调度器，使调度器在 setcontext 之后返回并执行 DONE 清理 */
        swapcontext(&c->ctx, &s_scheduler_ctx);
    }
}

/**
 * @brief 主动让出：当前协程进入就绪队列，调度器切到其他协程，实现协作式并发
 */
void latte_yield(void) {
    latte_coroutine_t* c = s_current;
    if (!c) return;
    c->state = LATTE_CORO_READY;
    swapcontext(&c->ctx, &s_scheduler_ctx);
}

/**
 * @brief 设置协程栈大小
 * @param size 栈大小（字节），最小8192字节
 */
void latte_coroutine_set_stack_size(size_t size) {
    if (size >= 8192)
        s_stack_size = size;
}

/* ---------- WaitGroup 实现：并发等待 ----------
 * count：待完成的子协程数。Add(n) 后启动 n 个子协程，每个结束时 Done() 减 1；
 * Wait() 在协程内循环 yield 直到 count==0，即"等所有子协程结束"。
 */

/**
 * @brief WaitGroup结构体，用于等待多个协程完成
 */
struct latte_waitgroup_t {
    int count;  /**< 等待完成的协程数量 */
};

/**
 * @brief 创建WaitGroup对象
 * @return 新创建的WaitGroup指针，失败返回NULL
 */
latte_waitgroup_t* latte_waitgroup_create(void) {
    latte_waitgroup_t* wg = (latte_waitgroup_t*)zmalloc(sizeof(latte_waitgroup_t));
    if (wg)
        wg->count = 0;
    return wg;
}

/**
 * @brief 释放WaitGroup对象
 * @param wg 要释放的WaitGroup指针
 */
void latte_waitgroup_free(latte_waitgroup_t* wg) {
    if (wg)
        zfree(wg);
}

/**
 * @brief 增加等待计数
 * @param wg WaitGroup指针
 * @param delta 要增加的数量（可以为负数表示减少）
 */
void latte_waitgroup_add(latte_waitgroup_t* wg, int delta) {
    if (wg)
        wg->count += delta;
}

/**
 * @brief 标记一个任务完成（计数减1）
 * @param wg WaitGroup指针
 */
void latte_waitgroup_done(latte_waitgroup_t* wg) {
    if (wg)
        latte_waitgroup_add(wg, -1);
}

/**
 * @brief 等待：在协程内反复 yield，直到 count 被其他协程的 Done() 减为 0，再返回
 * @param wg WaitGroup指针
 */
void latte_waitgroup_wait(latte_waitgroup_t* wg) {
    if (!wg) return;
    while (wg->count > 0)
        latte_yield();
}