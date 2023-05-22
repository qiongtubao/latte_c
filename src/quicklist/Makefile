WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=quicklist.o listpack.o
LIB_OBJ?= 
TEST_MAIN?=quicklist_test
MODULE?=quicklist
BUILD_INCLUDE=quicklist.h

include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk

include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/utils/lib.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/src/lzf/lib.mk
