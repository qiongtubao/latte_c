
# must set 
LATTE_LIB_WORKSPACE?=$(CURDIR)
MODULES?=sds zmalloc utils log ae server connection
BUILD_DIR?=out
ALL_OBJ=$(shell sh -c 'cat $(BUILD_DIR)/objs.list')
DEPENDENCY_TARGETS=


include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk

persist-settings: 
	-(cd ./deps && $(MAKE) $(DEPENDENCY_TARGETS))

LATTE_LIBS=
ifeq ($(MALLOC),jemalloc)
	LATTE_LIBS+= $(LATTE_LIB_WORKSPACE)/deps/jemalloc/lib/libjemalloc.a
endif
# $@：当前目标
# $^：当前规则中的所有依赖
# $<：依赖中的第一个
# $$：当前执行的进程编号
# $*：模式规则中所有%匹配的部分
# $?：模式规则中所有比目标更新的文件列表
%_test:
	cd src/$* && $(MAKE) test

%_asan_test:
	cd src/$* && $(MAKE) asan_test

%_module:
	cd src/$* && $(MAKE) SANITIZER=$(SANITIZER) install_lib BUILD_DIR=../../$(BUILD_DIR)

build: persist-settings
	mkdir -p $(BUILD_DIR)/lib
	mkdir -p $(BUILD_DIR)/out
	mkdir -p $(BUILD_DIR)/include
	rm -rf $(BUILD_DIR)/objs.list && touch $(BUILD_DIR)/objs.list
	$(foreach var,$(MODULES),$(MAKE) SANITIZER=$(SANITIZER) $(var)_module;)
	$(MAKE) latte_lib	

latte_lib: 	
	cd $(BUILD_DIR) && cp $(LATTE_LIB_WORKSPACE)/deps/jemalloc/lib/libjemalloc.a ./lib/ && $(AR) $(ARFLAGS) ./lib/liblatte.a  $(ALL_OBJ) $(LATTE_LIBS)

%_test_lib: build
	cd src/$* && $(MAKE) test_lib BUILD_DIR=../../$(BUILD_DIR)


clean_all:
	$(foreach var,$(MODULES),cd src/$(var) && $(MAKE) clean && cd ../../;)
	rm -rf $(BUILD_DIR)


./deps/jemalloc:
	-(cd ./deps && $(MAKE) jemalloc) 
	
test_all: ./deps/jemalloc
	make zmalloc_test
	make sds_test
	make log_test
	make dict_test
	make task_test
	make list_test
	make config_test
	make ae_test
	make mutex_test
	
