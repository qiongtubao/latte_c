LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=bitmap.o
LIB_OBJ?=
TEST_MAIN?=bitmap_test
MODULE?=bitmap
BUILD_INCLUDE=bitmap.h
LIB_MODULES?=zmalloc iterator
include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk
# echo $(foreach module,$(LIB_MODULES),$(LATTE_LIB_WORKSPACE)/src/$(module)/lib.mk)
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/iterator/lib.mk