LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=benchmark.o 
LIB_OBJ?=
TEST_MAIN?=benchmark_test
LIB_MODULES?=zmalloc utils sds list endianconv connection ae server
MODULE?=benchmark
BUILD_INCLUDE=benchmark.h

include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/list/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/ae/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/connection/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/rax/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/endianconv/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/server/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/utils/lib.mk
