LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=cuckoo.o
LIB_OBJ?=
TEST_MAIN?=cuckoo_test
LIB_MODULES?=zmalloc siphash 
MODULE?=cuckoo
BUILD_INCLUDE=cuckoo.h

include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/siphash/lib.mk