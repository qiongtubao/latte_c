WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=server.o 
LIB_OBJ?=
TEST_MAIN?=server_test
LIB_MODULES?=zmalloc sds list endianconv connection ae rax log
MODULE?=server
BUILD_INCLUDE=server.h

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
