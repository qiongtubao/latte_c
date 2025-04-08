
WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=stream.o 
TEST_MAIN?=stream_test
BUILD_INCLUDE?=stream.h 
MODULE?=stream
LIB_MODULES?=utils zmalloc sds dict rax


include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/rax/lib.mk