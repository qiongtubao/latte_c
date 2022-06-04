LATTE_LIB_DIR?=./out/lib


STD=-std=c99
FINAL_CFLAGS+= $(STD) -I./include -I../ -I./ -I../zmalloc -I../libs
FINAL_LIBS := -ldl -lc -lm $(FINAL_LIBS)

ifeq ($(USE_VALGRIND), yes) 
	VALGRIND = valgrind --track-origins=yes --show-reachable=no --show-possibly-lost=no --leak-check=full 
endif 
ifeq ($(MALLOC),jemalloc)
	DEPENDENCY_TARGETS+= jemalloc
	FINAL_CFLAGS+= -DUSE_JEMALLOC -I../../deps/jemalloc/include
	FINAL_LIBS := ../../deps/jemalloc/lib/libjemalloc.a  -lpthread $(FINAL_LIBS)
endif

LATTE_CC=$(CC) $(FINAL_CFLAGS)
NET_OBJ=net.o 
AR=ar
ARFLAGS=rcu
DEBUG=-g -ggdb


PREFIX?=.
INSTALL_DIR?=$(PREFIX)/bin
INSTALL=cp -rf
ifndef V
    define MAKE_INSTALL
        @printf '    %b %b\n' $(LINKCOLOR)INSTALL$(ENDCOLOR) $(BINCOLOR)$(1)$(ENDCOLOR) 1>&2
        @$(INSTALL) $(1) $(2)
    endef
else
    define MAKE_INSTALL
        $(INSTALL) $(1) $(2)
    endef
endif
.make-prerequisites:
	@touch $@

%.o: %.c .make-prerequisites
	echo $(LATTE_CC)
	$(LATTE_CC) $(DEBUG) -MMD -o $@ -c $< -DNET_TEST


# $(NET_LIB): $(NET_OBJ)
# 	@mkdir -p lib
# 	$(AR) $(ARFLAGS) $(NET_LIB) $(NET_OBJ)


all: $(NET_OBJ)

clean:
	rm -rf $(NET_LIB) $(NET_OBJ) ./net_test.o
	rm -rf net_test 
	rm -rf out
	sudo ../../scripts/clean_perf.sh 

test:  $(NET_OBJ) ./net_test.o zmalloc
	$(LATTE_CC)  -g -ggdb  -o  net_test  net_test.o net.o ../zmalloc/zmalloc.o -lm -ldl -lpthread -DNET_TEST $(FINAL_LIBS)
	@echo "================= test net start ================="
	$(VALGRIND) ./net_test
	@echo "=================  test net end  ================="

install: all
	@mkdir -p $(INSTALL_DIR)
	@mkdir -p $(INSTALL_DIR)/include 
	@mkdir -p $(INSTALL_DIR)/out
	cp net.h $(INSTALL_DIR)/include
	cp net.o $(INSTALL_DIR)/out

zmalloc:
	cd ../zmalloc && $(MAKE) MALLOC=$(MALLOC)

build_lib: all zmalloc
	mkdir -p out/lib
	$(AR) $(ARFLAGS) ./out/lib/liblatte.a net.o ../zmalloc/zmalloc.o

latte_lib_test: ./net_test.o 	
	$(LATTE_CC) -g -ggdb  -o  net_test net_test.o $(LATTE_LIB_DIR)/liblatte.a -lm -ldl -fno-omit-frame-pointer $(FINAL_LIBS) -DNET_TEST 
	@echo "================= test net start ================="
	$(VALGRIND) ./net_test
	@echo "=================  test net end  ================="

latte_perf_test: latte_lib_test
	sudo ../../scripts/clean_perf.sh  
	@echo "================= test net start ================="
	../../scripts/perf.sh ./net_test
	@echo "=================  test net end  ================="