WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=async.o
LIB_OBJ?=
TEST_MAIN?=async_test
MODULE?=async
BUILD_INCLUDE?=async.h 
LIB_MODULES?=zmalloc task dict sds


include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/task/lib.mk
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/sds/lib.mk