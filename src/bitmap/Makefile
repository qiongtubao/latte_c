WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=bitmap.o
LIB_OBJ?=
TEST_MAIN?=bitmap_test
MODULE?=bitmap
BUILD_INCLUDE=bitmap.h
LIB_MODULES?=zmalloc 
include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
# echo $(foreach module,$(LIB_MODULES),$(WORKSPACE)/src/$(module)/lib.mk)
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
