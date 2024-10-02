WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=value.o
LIB_OBJ?=
TEST_MAIN?=value_test
LIB_MODULES?=zmalloc sds iterator dict vector
MODULE?=value
BUILD_INCLUDE=value.h

include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/vector/lib.mk
include $(WORKSPACE)/src/iterator/lib.mk