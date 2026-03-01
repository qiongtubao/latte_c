#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "coroutine.h"

static double time_elapsed_sec(const struct timeval* start, const struct timeval* end) {
    return (end->tv_sec - start->tv_sec) + (end->tv_usec - start->tv_usec) / 1e6;
}

/* 用于断言的可写全局计数 */
static int s_step_count;
static int s_done_count;

static void task_count_yields(void* arg) {
    (void)arg;
    for (int i = 0; i < 3; i++) {
        s_step_count++;
        latte_yield();
    }
    s_done_count++;
}

static void task_no_yield(void* arg) {
    (void)arg;
    s_step_count++;
    s_done_count++;
}

static void task_two_yields(void* arg) {
    (void)arg;
    s_step_count++;
    latte_yield();
    s_step_count++;
    latte_yield();
    s_done_count++;
}

/* latte_go 能启动并跑完一个不 yield 的协程 */
int test_coroutine_go_run(void) {
    s_step_count = 0;
    s_done_count = 0;
    latte_go(task_no_yield, NULL);
    assert(s_step_count == 1 && s_done_count == 1);
    return 1;
}

/* 单个协程 yield 多次后正常结束 */
int test_coroutine_single_yield(void) {
    s_step_count = 0;
    s_done_count = 0;
    latte_go(task_count_yields, NULL);
    assert(s_step_count == 3 && s_done_count == 1);
    return 1;
}

/* 两个协程交替 yield，都能跑完 */
int test_coroutine_two_alternate(void) {
    s_step_count = 0;
    s_done_count = 0;
    latte_go(task_two_yields, NULL);
    latte_go(task_two_yields, NULL);
    assert(s_step_count == 4 && s_done_count == 2);
    return 1;
}

/* 多个 latte_go 依次启动，调度器能全部跑完 */
int test_coroutine_multi_go(void) {
    s_step_count = 0;
    s_done_count = 0;
    latte_go(task_count_yields, NULL);
    latte_go(task_count_yields, NULL);
    latte_go(task_no_yield, NULL);
    assert(s_step_count == 7 && s_done_count == 3);
    return 1;
}

/* 设置栈大小（在首次 latte_go 前）不崩溃 */
int test_coroutine_set_stack_size(void) {
    latte_coroutine_set_stack_size(64 * 1024);
    s_step_count = 0;
    s_done_count = 0;
    latte_go(task_no_yield, NULL);
    assert(s_done_count == 1);
    return 1;
}

/* ---------- WaitGroup 单测 ---------- */
static int s_wg_done_count; /* 有多少个子协程已调用 Done */

static void wg_worker(void* arg) {
    latte_waitgroup_t* wg = (latte_waitgroup_t*)arg;
    s_wg_done_count++;
    latte_waitgroup_done(wg);
}

static void wg_starter_two(void* arg) {
    latte_waitgroup_t* wg = (latte_waitgroup_t*)arg;
    latte_waitgroup_add(wg, 2);
    latte_go(wg_worker, wg);
    latte_go(wg_worker, wg);
    latte_waitgroup_wait(wg);
}

/* WaitGroup：启动 2 个子协程，Wait 后两者都已 Done */
int test_waitgroup_two_wait(void) {
    latte_waitgroup_t* wg = latte_waitgroup_create();
    assert(wg != NULL);
    s_wg_done_count = 0;
    latte_go(wg_starter_two, wg);
    assert(s_wg_done_count == 2);
    latte_waitgroup_free(wg);
    return 1;
}

/* WaitGroup：Add(0) 时 Wait 不阻塞 */
int test_waitgroup_zero_no_block(void) {
    latte_waitgroup_t* wg = latte_waitgroup_create();
    assert(wg != NULL);
    latte_waitgroup_add(wg, 0);
    latte_waitgroup_wait(wg); /* 应立刻返回 */
    latte_waitgroup_free(wg);
    return 1;
}

static void wg_starter_one(void* arg) {
    latte_waitgroup_t* wg = (latte_waitgroup_t*)arg;
    latte_waitgroup_add(wg, 1);
    latte_go(wg_worker, wg);
    latte_waitgroup_wait(wg);
}

/* WaitGroup：Add(1)，一个子协程 Done 后 Wait 返回 */
int test_waitgroup_one_worker(void) {
    latte_waitgroup_t* wg = latte_waitgroup_create();
    assert(wg != NULL);
    s_wg_done_count = 0;
    latte_go(wg_starter_one, wg);
    assert(s_wg_done_count == 1);
    latte_waitgroup_free(wg);
    return 1;
}

/* ---------- 并发 sleep：3 个协程分别 1s/2s/3s，总耗时应约 3–4s（并发） ---------- */
typedef struct {
    latte_waitgroup_t* wg;
    double duration_sec;
} sleep_arg_t;

static void sleep_worker(void* arg) {
    sleep_arg_t* a = (sleep_arg_t*)arg;
    struct timeval start, now;
    gettimeofday(&start, NULL);
    for (;;) {
        gettimeofday(&now, NULL);
        if (time_elapsed_sec(&start, &now) >= a->duration_sec)
            break;
        latte_yield();
    }
    latte_waitgroup_done(a->wg);
}

static void starter_sleep_three(void* arg) {
    latte_waitgroup_t* wg = (latte_waitgroup_t*)arg;
    latte_waitgroup_add(wg, 3);
    sleep_arg_t a1 = { wg, 1.0 }, a2 = { wg, 2.0 }, a3 = { wg, 3.0 };
    latte_go(sleep_worker, &a1);
    latte_go(sleep_worker, &a2);
    latte_go(sleep_worker, &a3);
    latte_waitgroup_wait(wg);
}

/* 3 个协程并发 sleep 1s/2s/3s，总耗时应在 3–4s（若串行则需 6s） */
int test_sleep_concurrent(void) {
    latte_waitgroup_t* wg = latte_waitgroup_create();
    assert(wg != NULL);
    struct timeval start, end;
    gettimeofday(&start, NULL);
    latte_go(starter_sleep_three, wg);
    gettimeofday(&end, NULL);
    double elapsed = time_elapsed_sec(&start, &end);
    latte_waitgroup_free(wg);
    assert(elapsed >= 3.0 && elapsed <= 4.5); /* 并发约 3s，留 0.5s 余量 */
    (void)elapsed; /* 避免未用告警（assert 在 release 可能被关） */
    return 1;
}

int test_api(void) {
    {
#ifdef LATTE_TEST
        /* private */
#endif
        test_cond("coroutine latte_go run",
            test_coroutine_go_run() == 1);
        test_cond("coroutine single yield",
            test_coroutine_single_yield() == 1);
        test_cond("coroutine two alternate",
            test_coroutine_two_alternate() == 1);
        test_cond("coroutine multi go",
            test_coroutine_multi_go() == 1);
        test_cond("coroutine set_stack_size",
            test_coroutine_set_stack_size() == 1);
        test_cond("waitgroup two wait",
            test_waitgroup_two_wait() == 1);
        test_cond("waitgroup zero no block",
            test_waitgroup_zero_no_block() == 1);
        test_cond("waitgroup one worker",
            test_waitgroup_one_worker() == 1);
        test_cond("sleep concurrent 1s+2s+3s in 3–4s",
            test_sleep_concurrent() == 1);
    } test_report()
    return 1;
}

int main(void) {
    test_api();
    return 0;
}
