
ALL_OBJ=zmalloc.o util.o
ALL_TEST=zmalloc_test util_test
BUILD_DIR?=out
ifeq ($(USE_SDS),yes)
	ALL_OBJ+=sds.o
	ALL_TEST+=sds_test
endif

STD=-std=c99
FINAL_CFLAGS+= $(STD) -I./include -I./

ifeq ($(MALLOC),tcmalloc)
	FINAL_CFLAGS+= -DUSE_TCMALLOC
	FINAL_LIBS+= -ltcmalloc
endif

ifeq ($(MALLOC),tcmalloc_minimal)
	FINAL_CFLAGS+= -DUSE_TCMALLOC
	FINAL_LIBS+= -ltcmalloc_minimal
endif

ifeq ($(MALLOC),jemalloc)
	DEPENDENCY_TARGETS+= jemalloc
	FINAL_CFLAGS+= -DUSE_JEMALLOC -I../../deps/jemalloc/include
	FINAL_LIBS := ../../deps/jemalloc/lib/libjemalloc.a $(FINAL_LIBS)
endif

LATTE_CC=$(CC) $(FINAL_CFLAGS)
LATTE_LIB=lib/liblatte.a
LATTE_OBJ=ctrip_gtid.o zmalloc.o util.o
AR=ar
ARFLAGS=rcu
DEBUG=-g -ggdb


PREFIX?=.
INSTALL_DIR?=$(PREFIX)/bin
INSTALL=cp -rf

.make-prerequisites:
	@touch $@

# %.o: %.c .make-prerequisites
# 	$(LATTE_CC) $(DEBUG) -MMD -o $@ -c $< -DCTRIP_GTID_TEST

zmalloc.o:
	mkdir -p ./$(BUILD_DIR) 
	cd zmalloc && $(MAKE) INSTALL_DIR=../$(BUILD_DIR) MALLOC=$(MALLOC) install

util.o:
	mkdir -p ./$(BUILD_DIR) 
	cd util && $(MAKE) INSTALL_DIR=../$(BUILD_DIR) MALLOC=$(MALLOC) install

sds.o:
	mkdir -p ./$(BUILD_DIR) 
	cd sds && $(MAKE) INSTALL_DIR=../$(BUILD_DIR) MALLOC=$(MALLOC) install


all: $(ALL_OBJ)
	echo "all"
	echo $(ALL_OBJ)
	mkdir -p $(BUILD_DIR)/lib
	cd $(BUILD_DIR)/out && $(AR) $(ARFLAGS) ../lib/liblatte.a $(FINAL_LIBS) $(ALL_OBJ)

zmalloc_test:
	cd zmalloc && $(MAKE) LATTE_LIB_DIR=../$(BUILD_DIR)/lib MALLOC=$(MALLOC) latte_lib_test

zmalloc_jemalloc_test:
	echo "latte zmalloc_jemalloc_test"
	$(MAKE) MALLOC=jemalloc all
	$(MAKE) MALLOC=jemalloc zmalloc_test

util_test:
	cd util && $(MAKE) LATTE_LIB_DIR=../$(BUILD_DIR)/lib MALLOC=$(MALLOC) latte_lib_test

sds_test:
	cd sds && $(MAKE) LATTE_LIB_DIR=../$(BUILD_DIR)/lib MALLOC=$(MALLOC) latte_lib_test

test: $(ALL_TEST)
