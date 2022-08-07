
# must set 
WORKSPACE?=$(CURDIR)
MODULES?=zmalloc sds
BUILD_DIR?=out
ALL_OBJ=$(shell sh -c 'cat $(BUILD_DIR)/objs.list')



include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk




# $@：当前目标
# $^：当前规则中的所有依赖
# $<：依赖中的第一个
# $$：当前执行的进程编号
# $*：模式规则中所有%匹配的部分
# $?：模式规则中所有比目标更新的文件列表
%_test:
	cd src/$* && $(MAKE) test

%_module:
	cd src/$* && $(MAKE) install_lib BUILD_DIR=../../$(BUILD_DIR)

build: 
	mkdir -p $(BUILD_DIR)/lib
	mkdir -p $(BUILD_DIR)/out
	rm -rf $(BUILD_DIR)/objs.list && touch $(BUILD_DIR)/objs.list
	$(foreach var,$(MODULES),$(MAKE) $(var)_module;)
	$(MAKE) latte_lib	

latte_lib: 	
	cd $(BUILD_DIR) && $(AR) $(ARFLAGS) ./lib/liblatte.a $(FINAL_LIBS) $(ALL_OBJ)

%_test_lib: build
	cd src/$* && $(MAKE) test_lib BUILD_DIR=../../$(BUILD_DIR)


clean_all:
	$(foreach var,$(MODULES),cd src/$(var) && $(MAKE) clean && cd ../../;)


test_all:
	make zmalloc_test
	make sds_test
	make task_test
	make dict_test
