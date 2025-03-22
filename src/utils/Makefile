WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=utils.o error.o
TEST_MAIN?=utils_test
BUILD_INCLUDE?=utils.h atomic.h config.h fmacros.h error.h
MODULE?=utils 

include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/sds/lib.mk