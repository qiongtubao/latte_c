LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=object.o object_string.o object_list.o object_hash.o object_zset.o object_set.o object_stream.o object_module.o
LIB_OBJ?=
LIB_MODULES=zset stream zset set dict list sds
TEST_MAIN?=object_test
MODULE?=object
BUILD_INCLUDE=object.h string.h list.h hash.h zset.h set.h stream.h module.h

include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/list/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/debug/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/zset/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/set/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/stream/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/rax/lib.mk