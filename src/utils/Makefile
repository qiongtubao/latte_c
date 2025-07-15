LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=utils.o error.o monotonic.o
TEST_MAIN?=utils_test
BUILD_INCLUDE?=utils.h atomic.h config.h fmacros.h error.h monotonic.h
MODULE?=utils 

include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk