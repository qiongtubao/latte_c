WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=process.o latte_signal.o pidfile.o
LIB_OBJ?= 
TEST_MAIN?=os_test
MODULE?=os
BUILD_INCLUDE=process.h latte_signal.h pidfile.h

include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk

include $(WORKSPACE)/mks/info.mk

include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/log/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
include $(WORKSPACE)/mks/latte.mk

