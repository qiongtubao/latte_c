LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=rdb.o
LIB_OBJ?=
LIB_MODULES=object sds log crc
TEST_MAIN?=rdb_test
MODULE?=rdb
BUILD_INCLUDE=rdb.h

include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/object/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/log/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/zset/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/set/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/stream/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/dict/lib.mk

LIB_OBJ+= ../object/object_hash.o ../object/object_stream.o
