# 开发日志 (History)

本文档记录 latte_c 项目的主要开发与变更。

---

## [Unreleased]

### RocksDB 封装 (src/rocksdb)
- 增加 `latte_rocksdb_compact_cf` 等接口与单测
- `latte_rocksdb_close` 完善资源释放顺序与注释

### 构建与测试
- Makefile 中 `LATTE_CFLAGS` 引号修复，避免 `-ftest-coverage` 被解析为 make 目标

---

## 历史记录

- **协程**：详见 [src/coroutine/History.md](src/coroutine/History.md)
- **RocksDB**：对 RocksDB 的 C 封装，支持多 CF、读写、flush、compact、close
- **基础模块**：sds、zmalloc、ae、dict、config、log 等，遵循项目命名与模块规范

---

*格式说明：按时间倒序，[Unreleased] 为当前未发布变更。*
