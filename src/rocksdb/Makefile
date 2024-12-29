
WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=latte_rocksdb.o 
LIB_OBJ?=
TEST_OBJS?=latte_rocksdb_test_set_get.o  latte_rocksdb_test_batch_set_get.o
TEST_MAIN?=latte_rocksdb_test
LIB_MODULES?=zmalloc
MODULE?=rocksdb
BUILD_INCLUDE=latte_rocksdb.h 

include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/mks/rocksdb.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/vector/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/mutex/lib.mk
include $(WORKSPACE)/src/log/lib.mk


