LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=lzf_c.o lzf_d.o
LIB_OBJ?= 
TEST_MAIN?=lzf_test
MODULE?=lzf
BUILD_INCLUDE=lzfP.h lzf.h

include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk

include $(LATTE_LIB_WORKSPACE)/mks/info.mk
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/dict/lib.mk

include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
