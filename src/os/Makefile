LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=process.o latte_signal.o pidfile.o
LIB_OBJ?= 
TEST_MAIN?=os_test
MODULE?=os
LIB_MODULES?=zmalloc utils 
BUILD_INCLUDE=process.h latte_signal.h pidfile.h

include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk

include $(LATTE_LIB_WORKSPACE)/mks/info.mk

include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/log/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/utils/lib.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk

