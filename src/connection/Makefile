WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=connection.o
LIB_OBJ?=
TEST_MAIN?=connection_test
MODULE?=connection
BUILD_INCLUDE=connection.h
LIB_MODULES?=zmalloc sds dict utils ae anet
include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
# echo $(foreach module,$(LIB_MODULES),$(WORKSPACE)/src/$(module)/lib.mk)
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
include $(WORKSPACE)/src/ae/lib.mk
include $(WORKSPACE)/src/anet/lib.mk