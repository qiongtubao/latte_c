WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=config.o
LIB_OBJ?=
TEST_MAIN?=config_test
MODULE?=config
BUILD_INCLUDE=config.h
LIB_MODULES?=zmalloc sds dict utils dict_plugins
include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
# echo $(foreach module,$(LIB_MODULES),$(WORKSPACE)/src/$(module)/lib.mk)
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/utils/lib.mk