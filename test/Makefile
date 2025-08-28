LATTE_LIB_WORKSPACE?=$(CURDIR)/../
LATTE_MALLOC=lib




DEBUG=-g 
#LIB_MODUELS="zset stream sds dict config list server io object lzf rax debug thread_single_object async_io ae utils"
LIB_MODUELS="zmalloc"

uname_S:= $(shell sh -c 'uname -s 2>/dev/null || echo not')
uname_M:= $(shell sh -c 'uname -s 2>/dev/null || echo not')

ifeq ($(uname_S),Linux)
	LATTE_MALLOC=jemalloc
	FINAL_SHARD_LIB=-lrt -ljemalloc -luring
endif

ifdef IOURING
	LATTE_BUILD+= IOURING=1
endif


#sanitizer 
ifdef SANITIZER
ifeq ($(SANITIZER),address)
   LATTE_MALLOC=libc
   CFLAGS+=-fsanitize=address -fno-sanitize-recover=all -fno-omit-frame-pointer
   LDFLAGS+=-fsanitize=address
else
ifeq ($(SANITIZER),undefined)
   LATTE_MALLOC=libc
   CFLAGS+=-fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
   LDFLAGS+=-fsanitize=undefined
else
ifeq ($(SANITIZER),thread)
   CFLAGS+=-fsanitize=thread -fno-sanitize-recover=all -fno-omit-frame-pointer
   LDFLAGS+=-fsanitize=thread
else
    $(error "unknown sanitizer=${SANITIZER}")
endif
endif
endif
endif

FINAL_CC_CFLAGS=$(STD) $(WARN) $(OPT) $(DEBUG) $(CFLAGS) $(LATTE_CFLAGS)
FINAL_CXX_CFLAGS=$(CXX_STD) $(WARN) $(OPT) $(DEBUG) $(CFLAGS) $(LATTE_CFLAGS)

LATTE_CC=$(QUIET_CC)$(CC) $(FINAL_CC_CFLAGS) $(FINAL_CC_LIBS)
LATTE_GCC=$(QUIET_CC)$(CC) $(FINAL_CC_CFLAGS) $(FINAL_CC_LIBS)
LATTE_LD=$(QUIET_LINK)$(CC) $(FINAL_LDFLAGS) $(FINAL_CC_LIBS)



$(LATTE_LIB_WORKSPACE)/out/lib/liblatte.a:
	cd ../ && $(MAKE) SANITIZER=$(SANITIZER) build $(LATTE_BUILD) MALLOC=$(LATTE_MALLOC) MODULES=$(LIB_MODUELS) 

%.o: %.c $(LATTE_LIB_WORKSPACE)/out/lib/liblatte.a
	$(LATTE_CC) $(DEBUG) -MMD -o $@ -c $< -I$(LATTE_LIB_WORKSPACE)/out/include 


test: $(LATTE_LIB_WORKSPACE)/out/lib/liblatte.a lib_asan_test.o
	$(LATTE_CC) $(DEBUG) -o lib_asan_test lib_asan_test.o -I$(LATTE_LIB_WORKSPACE)/out/include -I$(LATTE_LIB_WORKSPACE)/deps/jemalloc/include -L$(LATTE_LIB_WORKSPACE)/out/lib -lm  -ldl -lz -lstdc++ -lpthread -llatte $(FINAL_SHARD_LIB) $(MAC_M1_CFLAGS)

clean:
	cd ../ && rm -rf out && make clean_all