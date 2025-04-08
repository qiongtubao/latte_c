WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=object.o object_string.o object_list.o object_hash.o object_zset.o object_set.o object_stream.o object_module.o
LIB_OBJ?=
LIB_MODULES=zset stream zset set dict list sds
TEST_MAIN?=object_test
MODULE?=object
BUILD_INCLUDE=object.h string.h list.h hash.h zset.h set.h stream.h module.h

include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/debug/lib.mk
include $(WORKSPACE)/src/zset/lib.mk
include $(WORKSPACE)/src/set/lib.mk
include $(WORKSPACE)/src/stream/lib.mk
include $(WORKSPACE)/src/rax/lib.mk