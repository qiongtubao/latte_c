
LATTE_LIB_DIR?=./out/lib


STD=-std=c99

FINAL_CFLAGS+= $(STD) -I../ -I../zmalloc
FINAL_LIBS := -ldl -lc -lm $(FINAL_LIBS)
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

test:  $(OBJS) ./dict_test.o zmalloc
	$(LATTE_CC)  -g -ggdb  -o  dict_test  dict_test.o $(OBJS) ../zmalloc/zmalloc.o -lm -ldl -lpthread -DDICT_TEST $(FINAL_LIBS)
	@echo "================= test sds start ================="
	./dict_test
	@echo "=================  test sds end  ================="