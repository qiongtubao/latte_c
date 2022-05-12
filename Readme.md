

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