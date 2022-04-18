

STD=-std=c99
FINAL_CFLAGS+= $(STD) -I./include -I./ -I../

ifeq ($(MALLOC),jemalloc)
	DEPENDENCY_TARGETS+= jemalloc
	FINAL_CFLAGS+= -DUSE_JEMALLOC -I../../deps/jemalloc/include
	FINAL_LIBS := ../../deps/jemalloc/lib/libjemalloc.a $(FINAL_LIBS)
endif

LATTE_CC=$(CC) $(FINAL_CFLAGS)
ZMALLOC_OBJ=zmalloc.o 
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
	$(LATTE_CC) $(DEBUG) -MMD -o $@ -c $< -DZMALLOC_TEST 



# $(ZMALLOC_LIB): $(ZMALLOC_OBJ)
# 	@mkdir -p lib
# 	$(AR) $(ARFLAGS) $(ZMALLOC_LIB) $(ZMALLOC_OBJ)


all: $(ZMALLOC_OBJ)

clean:
	rm -rf $(ZMALLOC_LIB) $(ZMALLOC_OBJ) ./zmalloc_test.o
	rm -rf zmalloc_test 
	rm -rf out

test:$(ZMALLOC_OBJ) zmalloc_test.o
	$(LATTE_CC)  -g -ggdb  -o  zmalloc_test  zmalloc_test.o zmalloc.o -lm -ldl -DZMALLOC_TEST $(FINAL_LIBS)
	@echo "================= test zmalloc start ================="
	./zmalloc_test
	@echo "================= test zmalloc end ================="

jemalloc_test:
	$(MAKE) MALLOC=jemalloc test

jemalloc_lib_test:
	$(MAKE) MALLOC=jemalloc latte_lib_test 

install: all
	@mkdir -p $(INSTALL_DIR)
	@mkdir -p $(INSTALL_DIR)/include 
	@mkdir -p $(INSTALL_DIR)/out
	cp zmalloc.h $(INSTALL_DIR)/include
	cp solarisfixes.h $(INSTALL_DIR)/include
	cp fmacros.h $(INSTALL_DIR)/include
	cp atomicvar.h $(INSTALL_DIR)/include
	cp zmalloc.o $(INSTALL_DIR)/out

build_lib: all zmalloc
	mkdir -p out/lib
	$(AR) $(ARFLAGS) ./out/lib/liblatte.a zmalloc.o 

latte_lib_test: zmalloc_test.o	
	$(LATTE_CC) -g -ggdb  -o  zmalloc_test zmalloc_test.o  $(LATTE_LIB_DIR)/liblatte.a  -lm -ldl -DLATTE_TEST $(FINAL_LIBS)
	@echo "================= test zmalloc start ================="
	./zmalloc_test
	@echo "=================  test zmalloc end  ================="
