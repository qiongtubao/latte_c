# Latte C

C 语言基础库与组件集合，用于构建高性能服务。

---

# 功能概览（模块与功能）

| 模块 | 路径 | 功能简述 |
|------|------|----------|
| **协程** | [src/coroutine](src/coroutine/README.md) | 类 Go 协程、协作式调度、WaitGroup 并发等待 |
| **RocksDB** | src/rocksdb | RocksDB C 封装：多 CF、读写、flush、compact、close |
| **内存** | src/zmalloc | 统一内存分配封装，可切换 libc/jemalloc |
| **字符串** | src/sds | 动态字符串，多类型 sdshdr5/8/16/32/64 |
| **事件循环** | src/ae | 事件驱动与定时器 |
| **字典** | src/dict | 哈希表等字典结构 |
| **配置** | src/config | 配置加载与解析 |
| **日志** | src/log | 日志输出 |
| **网络** | src/anet | 网络封装 |
| **对象/其他** | src/object, list, set, stream, pool, cache, json, ... | 各类数据结构与工具 |

更细的功能与 API 见各模块目录下的 README（若有，如协程见 [src/coroutine/README.md](src/coroutine/README.md)）。项目开发与变更记录见 [History.md](History.md)。

---

# 命名规范
c 语言系统判断在 utils/sys_config.h 


类名  
```c
  typedef struct {
  
  } xxx_t;
```

枚举
```c
  typedef enum {

  } xxx_enum;
```

方法名 
```c
  typedef int xxx_xxx_func(void* v)
  xxx_t xxx_new(xxx),void xxx_delete(xxx_t* xxx)  // 调用xxx_create
  xxx_t xxx_create(void) , void xxx_release(xxx_t xxx)

```


静态对象
global_xxx

模块
初始化    xxx_module_init
释放      xxx_module_destroy


# 模块规范

1. add module 
    1. cd src &&  mkdir xxx
      Makefile
      xxx.h
      xxx.c 
      xxx_test.c
    2. make test
2.  add install
    1.  cp xxx.h  => include 
    2.  cp xxx.o  => out
    3.  ar xxx.o  => liblatte.a
3. make xxx_test
   1. make MALLOC=libc xxx_test
   2. make MALLOC=jemalloc xxx_test

# 引入新库
  1. 存放在deps 
  2. 修改deps/Makefile 使其能编译
  3. 引入
    ``` bash
      FINAL_CC_CFLAGS+= -DUSE_JSONC -I../../deps/json-c/ -I../../deps/json-c/build
      FINAL_CXX_CFLAGS+= -DUSE_JSONC -I../../deps/json-c/ -I../../deps/json-c/build
      FINAL_CC_LIBS:= ../../deps/json-c/build/libjson-c.a $(FINAL_CC_LIBS)
      FINAL_CXX_LIBS:= ../../deps/json-c/build/libjson-c.a $(FINAL_CXX_LIBS)
    ```


# 测试
## macos
make mac_test -j4

## linux
make asan_test -j4