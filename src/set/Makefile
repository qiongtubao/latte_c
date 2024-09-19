WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?= lockSet.o avlSet.o hashSet.o set.o
LIB_OBJ?=
TEST_MAIN?=set_test
LIB_MODULES?=zmalloc mutex sds dict dict_plugins tree iterator
MODULE?=set
BUILD_INCLUDE=set.h lockSet.h avlSet.h hashSet.h

include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/dict_plugins/lib.mk
include $(WORKSPACE)/src/mutex/lib.mk
include $(WORKSPACE)/src/tree/lib.mk
include $(WORKSPACE)/src/iterator/lib.mk



