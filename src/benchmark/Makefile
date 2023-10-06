WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=benchmark.o 
LIB_OBJ?=
TEST_MAIN?=benchmark_test
LIB_MODULES?=zmalloc sds list endianconv connection ae server
MODULE?=benchmark
BUILD_INCLUDE=benchmark.h

include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/ae/lib.mk
include $(WORKSPACE)/src/connection/lib.mk
include $(WORKSPACE)/src/rax/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/endianconv/lib.mk
include $(WORKSPACE)/src/server/lib.mk
