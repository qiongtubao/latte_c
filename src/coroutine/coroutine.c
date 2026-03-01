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

enum {
    LATTE_CORO_READY,
    LATTE_CORO_RUNNING,
    LATTE_CORO_DONE
};

typedef struct latte_coroutine_t {
    ucontext_t ctx;
    void* stack;
    int state;
    latte_coroutine_fn fn;
    void* arg;
    struct latte_coroutine_t* next;
} latte_coroutine_t;

/* 默认栈大小 256KB */
#define LATTE_CORO_DEFAULT_STACK_SIZE (256 * 1024)
static size_t s_stack_size = LATTE_CORO_DEFAULT_STACK_SIZE;

static ucontext_t s_scheduler_ctx;
static latte_coroutine_t* s_ready_head = NULL;  /* 就绪队列头，并发调度即从此队列轮转取协程 */
static latte_coroutine_t* s_ready_tail = NULL;
static latte_coroutine_t* s_current = NULL;   /* 当前正在运行的协程 */
/* 协程内调用 latte_go 时不在协程栈上 getcontext，由调度器创建新协程 */
static latte_coroutine_fn s_pending_fn = NULL;
static void* s_pending_arg = NULL;

static void push_ready(latte_coroutine_t* c) {
    c->next = NULL;
    if (s_ready_tail)
        s_ready_tail->next = c;
    else
        s_ready_head = c;
    s_ready_tail = c;
}

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
 * 调度循环：驱动并发。每次从就绪队列取一个协程执行，该协程 yield 或结束时回到此处，
 * 再取下一个，直到队列空。所有“等待”最终都通过 yield 把 CPU 让给其他协程。
 */
static void run_scheduler(void) {
    for (;;) {
        /* 统一返回点：协程 yield/完成时 swapcontext 回到此处 */
        getcontext(&s_scheduler_ctx);
        if (s_current != NULL) {
            if (s_current->state == LATTE_CORO_READY)
                push_ready(s_current);
            else if (s_current->state == LATTE_CORO_DONE) {
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
            latte_coroutine_t* c = (latte_coroutine_t*)zmalloc(sizeof(latte_coroutine_t));
            memset(c, 0, sizeof(*c));
            c->fn = s_pending_fn;
            c->arg = s_pending_arg;
            s_pending_fn = NULL;
            s_pending_arg = NULL;
            c->state = LATTE_CORO_READY;
            c->stack = zmalloc(s_stack_size);
            if (c->stack) {
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
        if (s_ready_head == NULL)
            break;
        latte_coroutine_t* c = pop_ready();
        s_current = c;
        c->state = LATTE_CORO_RUNNING;
        setcontext(&c->ctx);
    }
}

void latte_go(latte_coroutine_fn fn, void* arg) {
    if (s_current != NULL) {
        /* 在协程内调用：不在此处 getcontext，交给调度器创建，避免覆盖当前协程 ctx */
        s_pending_fn = fn;
        s_pending_arg = arg;
        push_ready(s_current);
        swapcontext(&s_current->ctx, &s_scheduler_ctx);
        return;
    }
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

/** 主动让出：当前协程进入就绪队列，调度器切到其他协程，实现协作式并发。 */
void latte_yield(void) {
    latte_coroutine_t* c = s_current;
    if (!c) return;
    c->state = LATTE_CORO_READY;
    swapcontext(&c->ctx, &s_scheduler_ctx);
}

void latte_coroutine_set_stack_size(size_t size) {
    if (size >= 8192)
        s_stack_size = size;
}

/* ---------- WaitGroup 实现：并发等待 ----------
 * count：待完成的子协程数。Add(n) 后启动 n 个子协程，每个结束时 Done() 减 1；
 * Wait() 在协程内循环 yield 直到 count==0，即“等所有子协程结束”。
 */
struct latte_waitgroup_t {
    int count;
};

latte_waitgroup_t* latte_waitgroup_create(void) {
    latte_waitgroup_t* wg = (latte_waitgroup_t*)zmalloc(sizeof(latte_waitgroup_t));
    if (wg)
        wg->count = 0;
    return wg;
}

void latte_waitgroup_free(latte_waitgroup_t* wg) {
    if (wg)
        zfree(wg);
}

void latte_waitgroup_add(latte_waitgroup_t* wg, int delta) {
    if (wg)
        wg->count += delta;
}

void latte_waitgroup_done(latte_waitgroup_t* wg) {
    if (wg)
        latte_waitgroup_add(wg, -1);
}

/** 等待：在协程内反复 yield，直到 count 被其他协程的 Done() 减为 0，再返回。 */
void latte_waitgroup_wait(latte_waitgroup_t* wg) {
    if (!wg) return;
    while (wg->count > 0)
        latte_yield();
}
