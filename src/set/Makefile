LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?= avl_set.o hash_set.o set.o int_set.o
LIB_OBJ?=
TEST_MAIN?=set_test
LIB_MODULES?= zmalloc sds dict tree iterator
MODULE?=set
BUILD_INCLUDE= set.h avl_set.h hash_set.h

include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk

include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/dict/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/tree/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/iterator/lib.mk



