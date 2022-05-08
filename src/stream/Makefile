LATTE_LIB_DIR?=./out/lib


STD=-std=c99
FINAL_CFLAGS+= $(STD) -I./include -I../ -I./ -I../zmalloc -I../sds -I../util
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
STREAM_OBJ=stream.o bufferStream.o fileStream.o fdStream.o mmapStream.o
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
	$(LATTE_CC) $(DEBUG) -MMD -o $@ -c $< -Dstream_test


# $(SDS_LIB): $(SDS_OBJ)
# 	@mkdir -p lib
# 	$(AR) $(ARFLAGS) $(SDS_LIB) $(SDS_OBJ)


all: $(STREAM_OBJ)

clean:
	rm -rf $(STREAM_OBJ) ./stream_test.o
	rm -rf stream_test 
	rm -rf out
	sudo ../../scripts/clean_perf.sh 



test:  $(STREAM_OBJ) ./stream_test.o zmalloc sds util
	$(LATTE_CC)  -g -ggdb  -o  stream_test  stream_test.o $(STREAM_OBJ) ../zmalloc/zmalloc.o ../sds/sds.o ../util/util.o -lm -ldl -lpthread -DSTREAM_TEST $(FINAL_LIBS)
	@echo "================= test sds start ================="
	$(VALGRIND) ./stream_test
	@echo "=================  test sds end  ================="

install: all
	@mkdir -p $(INSTALL_DIR)
	@mkdir -p $(INSTALL_DIR)/include 
	@mkdir -p $(INSTALL_DIR)/out
	cp sds.h $(INSTALL_DIR)/include
	cp sds.o $(INSTALL_DIR)/out

zmalloc:
	cd ../zmalloc && $(MAKE) MALLOC=$(MALLOC)

util:
	cd ../util && $(MAKE) MALLOC=$(MALLOC)

sds:
	cd ../sds && $(MAKE) MALLOC=$(MALLOC)

build_lib: all zmalloc sds 
	mkdir -p out/lib
	$(AR) $(ARFLAGS) ./out/lib/liblatte.a $(STREAM_OBJ) ../sds/sds.o ../zmalloc/zmalloc.o ../util/util.o 

latte_lib_test: ./stream_test.o 	
	$(LATTE_CC) -g -ggdb  -o  stream_test stream_test.o $(LATTE_LIB_DIR)/liblatte.a -lm -ldl -fno-omit-frame-pointer $(FINAL_LIBS) -Dstream_test 
	@echo "================= test sds start ================="
	$(VALGRIND) ./stream_test
	@echo "=================  test sds end  ================="

latte_perf_test: latte_lib_test
	sudo ../../scripts/clean_perf.sh  
	@echo "================= test sds start ================="
	../../scripts/perf.sh ./stream_test
	@echo "=================  test sds end  ================="