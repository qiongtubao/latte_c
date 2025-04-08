WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=latte_debug.o 
TEST_MAIN?=latte_debug_test
MODULE?=debug
LIB_MODULES?=zmalloc log
BUILD_INCLUDE=latte_debug.h 
include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/iterator/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/cmp/lib.mk
include $(WORKSPACE)/src/log/lib.mk