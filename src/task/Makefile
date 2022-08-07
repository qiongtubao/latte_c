WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=task.o setcpuaffinity.o
LIB_OBJ?=../zmalloc/zmalloc.o ../list/list.o 
TEST_MAIN?=task_test

include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
include $(WORKSPACE)/src/list/lib.mk

