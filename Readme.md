# 命名规范
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
```

静态对象
global_xxx


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