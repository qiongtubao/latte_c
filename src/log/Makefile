WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=log.o
LIB_OBJ?=
TEST_MAIN?=log_test
LIB_MODULES?=zmalloc sds utils 
MODULE?=log
BUILD_INCLUDE=log.h

include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
