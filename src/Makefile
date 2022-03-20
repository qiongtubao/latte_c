
ALL_OBJ=zmalloc.o util.o
ALL_TEST=zmalloc_test util_test
BUILD_DIR?=out
ifeq ($(USE_SDS),yes)
	ALL_OBJ+=sds.o
	ALL_TEST+=sds_test
endif

STD=-std=c99
FINAL_CFLAGS+= $(STD) -I./include -I./

LATTE_CC=$(CC) $(FINAL_CFLAGS)
LATTE_LIB=lib/liblatte.a
LATTE_OBJ=ctrip_gtid.o zmalloc.o util.o
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

# %.o: %.c .make-prerequisites
# 	$(LATTE_CC) $(DEBUG) -MMD -o $@ -c $< -DCTRIP_GTID_TEST

zmalloc.o:
	mkdir -p ./$(BUILD_DIR) 
	cd zmalloc && $(MAKE) INSTALL_DIR=../$(BUILD_DIR) install

util.o:
	mkdir -p ./$(BUILD_DIR) 
	cd util && $(MAKE) INSTALL_DIR=../$(BUILD_DIR) install

sds.o:
	mkdir -p ./$(BUILD_DIR) 
	cd sds && $(MAKE) INSTALL_DIR=../$(BUILD_DIR) install


all: $(ALL_OBJ)
	echo "all"
	echo $(ALL_OBJ)
	mkdir -p $(BUILD_DIR)/lib
	cd $(BUILD_DIR)/out && $(AR) $(ARFLAGS) ../lib/liblatte.a $(ALL_OBJ)

zmalloc_test:
	cd zmalloc && $(MAKE) LATTE_LIB_DIR=../$(BUILD_DIR)/lib latte_lib_test

util_test:
	cd util && $(MAKE) LATTE_LIB_DIR=../$(BUILD_DIR)/lib latte_lib_test

sds_test:
	cd sds && $(MAKE) LATTE_LIB_DIR=../$(BUILD_DIR)/lib latte_lib_test

test: $(ALL_TEST)
