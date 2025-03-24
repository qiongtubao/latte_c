WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=server.o client.o
LIB_OBJ?=
TEST_MAIN?=server_test
LIB_MODULES?=zmalloc sds list endianconv connection ae rax log value func_task
MODULE?=server
BUILD_INCLUDE=server.h client.h

include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/ae/lib.mk
include $(WORKSPACE)/src/connection/lib.mk
include $(WORKSPACE)/src/rax/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/endianconv/lib.mk
include $(WORKSPACE)/src/log/lib.mk
include $(WORKSPACE)/src/value/lib.mk
include $(WORKSPACE)/src/func_task/lib.mk