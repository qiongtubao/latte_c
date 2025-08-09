LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=ae.o 
LIB_OBJ?=
TEST_MAIN?=ae_test
MODULE?=ae
BUILD_INCLUDE?=ae.h 
LIB_MODULES?=anet zmalloc dict log func_task utils

include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/log/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/anet/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/utils/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/list/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/func_task/lib.mk

ifdef IOURING
    include $(LATTE_LIB_WORKSPACE)/mks/sys_config.mk
endif

ifdef SELECT
    FINAL_CC_CFLAGS+=-DHAVE_SELECT 
	FINAL_CC_LDFLAGS+=-DHAVE_SELECT
endif