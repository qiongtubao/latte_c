WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=dict.o siphash.o dict_plugins.o
LIB_OBJ?=
TEST_MAIN?=dict_test
MODULE?=dict
LIB_MODULES?=zmalloc iterator cmp
BUILD_INCLUDE=dict.h dict_plugins.h
include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/iterator/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/cmp/lib.mk
