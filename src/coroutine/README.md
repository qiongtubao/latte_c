# 协程模块 (coroutine)

类 Go 协程：`latte_go` / `latte_yield`，协作式调度、每协程独立栈；提供 WaitGroup 并发等待。

---

## 功能概览

| 能力 | 说明 |
|------|------|
| **并发** | 单线程内多协程协作式并发；通过 `latte_yield()` 让出，调度器切换其他就绪协程 |
| **等待** | WaitGroup：主协程 `Add(n)` 后启动 n 个子协程，子协程结束时 `Done()`，主协程 `Wait()` 阻塞直到计数归零 |

## API 简述

- `latte_go(fn, arg)` — 启动一个协程
- `latte_yield()` — 主动让出 CPU（仅可在协程内调用）
- `latte_coroutine_set_stack_size(size)` — 设置协程栈大小（须在首次 `latte_go` 前）
- `latte_waitgroup_create` / `latte_waitgroup_free` — 创建/释放 WaitGroup
- `latte_waitgroup_add(wg, delta)` / `latte_waitgroup_done(wg)` / `latte_waitgroup_wait(wg)` — 并发等待

更细的语义与用法见 `coroutine.h` 内注释。开发与变更记录见 [History.md](History.md)。

## 依赖与测试

- 依赖：POSIX ucontext（macOS / Linux）；项目内 zmalloc
- 单测：`make coroutine_test` 或从项目根目录 `make coroutine_test`
