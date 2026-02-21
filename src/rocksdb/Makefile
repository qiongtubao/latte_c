LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=latte_rocksdb.o
LIB_OBJ?=
TEST_MAIN?=latte_rocksdb_test
LIB_MODULES?=zmalloc sds error dict utils
MODULE?=rocksdb
BUILD_INCLUDE=latte_rocksdb.h 

include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/rocksdb.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/error/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/dict/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/utils/lib.mk


