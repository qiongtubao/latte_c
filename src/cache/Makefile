LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=lru_cache.o 
LIB_OBJ?=
TEST_MAIN?=cache_test
MODULE?=cache
LIB_MODULES?=zmalloc utils dict  list sds
BUILD_INCLUDE=lru_cache.h
include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/list/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/dict/lib.mk