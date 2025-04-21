WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=ae.o 
LIB_OBJ?=
TEST_MAIN?=ae_test
MODULE?=ae
BUILD_INCLUDE?=ae.h 
LIB_MODULES?=anet zmalloc dict log func_task utils

include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/log/lib.mk
include $(WORKSPACE)/src/anet/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/func_task/lib.mk
