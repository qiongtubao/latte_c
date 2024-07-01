WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=lzf_c.o lzf_d.o
LIB_OBJ?= 
TEST_MAIN?=lzf_test
MODULE?=lzf
BUILD_INCLUDE=lzfP.h

include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk

include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/mks/latte.mk
