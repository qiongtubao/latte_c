WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=mem.o 
LIB_OBJ?=
TEST_MAIN?=pool_test
MODULE?=pool
LIB_MODULES?=zmalloc utils list dict mutex log set
BUILD_INCLUDE=mem.h
include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/mutex/lib.mk
include $(WORKSPACE)/src/log/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
include $(WORKSPACE)/src/set/lib.mk