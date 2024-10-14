WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=lru_cache.o 
LIB_OBJ?=
TEST_MAIN?=cache_test
MODULE?=cache
LIB_MODULES?=zmalloc utils dict  list sds
BUILD_INCLUDE=lru_cache.h
include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/dict/lib.mk