LATTE_LIB_DIR?=./out/lib


STD=-std=c99
FINAL_CFLAGS+= $(STD) -I./include -I../ -I./ -I../zmalloc

LATTE_CC=$(CC) $(FINAL_CFLAGS)
UTIL_OBJ=util.o 
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
	$(LATTE_CC) $(DEBUG) -MMD -o $@ -c $< -DUTIL_TEST


# $(UTIL_LIB): $(UTIL_OBJ)
# 	@mkdir -p lib
# 	$(AR) $(ARFLAGS) $(UTIL_LIB) $(UTIL_OBJ)


all: $(UTIL_OBJ)

clean:
	rm -rf $(UTIL_LIB) $(UTIL_OBJ) ./util_test.o
	rm -rf util_test 
	rm -rf out

test:  $(UTIL_OBJ) util_test.o	
	$(LATTE_CC)  -g -ggdb  -o  util_test  util_test.o util.o -lm -ldl -DUTIL_TEST
	./util_test

install: all
	@mkdir -p $(INSTALL_DIR)
	@mkdir -p $(INSTALL_DIR)/include 
	@mkdir -p $(INSTALL_DIR)/out
	cp util.h $(INSTALL_DIR)/include
	cp util.o $(INSTALL_DIR)/out

zmalloc:
	cd ../zmalloc && $(MAKE) 

build_lib: all 
	mkdir -p out/lib
	$(AR) $(ARFLAGS) ./out/lib/liblatte.a util.o 

latte_lib_test: util_test.o
	$(LATTE_CC) -g -ggdb  -o  util_test util_test.o $(LATTE_LIB_DIR)/liblatte.a -lm -ldl -DUTIL_TEST
	@echo "================= test util start ================="
	./util_test
	@echo "=================  test util end  ================="
