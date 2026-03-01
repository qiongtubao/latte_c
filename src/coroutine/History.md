# 协程模块开发日志 (History)

本文档记录 src/coroutine 的开发与变更。

---

## [Unreleased]

- **WaitGroup**：增加并发等待能力，API 类似 Go `sync.WaitGroup`
  - `latte_waitgroup_create` / `latte_waitgroup_free`
  - `latte_waitgroup_add` / `latte_waitgroup_done` / `latte_waitgroup_wait`
- **协程内 latte_go**：从协程内部调用 `latte_go` 时改为“延迟创建”，由调度器在自身栈上执行 `getcontext` 创建新协程，避免覆盖当前协程 ctx 导致 Bus error
- **单测**：按 sds_test 规范编写，使用 testhelp/testassert；调度器统一返回点修复后支持多次/嵌套 `latte_go`
- **并发 sleep 单测**：3 个协程分别模拟 sleep 1s/2s/3s，总耗时为 3–4s，验证协作式并发效果（串行需 6s）
- **注释**：在头文件与实现中增加“并发”“等待”等核心注释

---

## 历史记录

- 基于 POSIX ucontext 实现类 Go 的 `latte_go` / `latte_yield`，每协程独立栈、协作式调度

---

*格式说明：按时间倒序，[Unreleased] 为当前未发布变更。*
