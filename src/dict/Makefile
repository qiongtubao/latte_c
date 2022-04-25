
LATTE_LIB_DIR?=./out/lib


STD=-std=c99

FINAL_CFLAGS+= $(STD) -I../ -I../zmalloc
FINAL_LIBS := -ldl -lc -lm $(FINAL_LIBS)

ifeq ($(USE_VALGRIND), yes) 
	VALGRIND = valgrind --track-origins=yes --show-reachable=no --show-possibly-lost=no --leak-check=full 
endif 
ifeq ($(MALLOC),jemalloc)
	DEPENDENCY_TARGETS+= jemalloc
	FINAL_CFLAGS+= -DUSE_JEMALLOC -I../../deps/jemalloc/include
	FINAL_LIBS :=  ../../deps/jemalloc/lib/libjemalloc.a -lpthread $(FINAL_LIBS)
endif

LATTE_CC=$(CC) $(FINAL_CFLAGS)
AR=ar
ARFLAGS=rcu
DEBUG=-g -ggdb

PREFIX?=.
INSTALL_DIR?=$(PREFIX)/bin
INSTALL=cp -rf
define MAKE_INSTALL
	$(INSTALL) $(1) $(2)
endef


OBJS=mt19937-64.o siphash.o dict.o 
.make-prerequisites:
	@touch $@

%.o: %.c .make-prerequisites
	echo $(LATTE_CC)
	$(LATTE_CC) $(DEBUG) -MMD -o $@ -c $< -DDICT_TEST

zmalloc:
	cd ../zmalloc && $(MAKE) MALLOC=$(MALLOC)

all: $(OBJS) zmalloc

clean:
	rm -rf $(OBJS) ./dict_test.o
	rm -rf *.d
	rm -rf dict_test 
	rm -rf out

install: all
	@mkdir -p $(INSTALL_DIR)
	@mkdir -p $(INSTALL_DIR)/include 
	@mkdir -p $(INSTALL_DIR)/out
	cp dict.h $(INSTALL_DIR)/include
	cp dict.o $(INSTALL_DIR)/out
	cp mt19937-64.o $(INSTALL_DIR)/out 
	cp siphash.o $(INSTALL_DIR)/out 

latte_lib_test: dict_test.o
	$(LATTE_CC) -g -ggdb  -o  dict_test dict_test.o $(LATTE_LIB_DIR)/liblatte.a -lm -ldl  $(FINAL_LIBS) -DDICT_TEST 
	@echo "================= test sds start ================="
	$(VALGRIND) ./dict_test
	@echo "=================  test sds end  ================="

test:  $(OBJS) ./dict_test.o zmalloc
	$(LATTE_CC)  -g -ggdb  -o  dict_test  dict_test.o $(OBJS) ../zmalloc/zmalloc.o -lm -ldl -lpthread -DDICT_TEST $(FINAL_LIBS)
	@echo "================= test sds start ================="
	$(VALGRIND) ./dict_test
	@echo "=================  test sds end  ================="