LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=dict.o siphash.o dict_plugins.o
LIB_OBJ?=
TEST_MAIN?=dict_test
MODULE?=dict
LIB_MODULES?=zmalloc iterator cmp
BUILD_INCLUDE=dict.h dict_plugins.h
include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/iterator/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/cmp/lib.mk
