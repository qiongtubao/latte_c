LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=connection.o
LIB_OBJ?=
TEST_MAIN?=connection_test
MODULE?=connection
BUILD_INCLUDE=connection.h
LIB_MODULES?=zmalloc sds dict utils ae anet time
include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk
# echo $(foreach module,$(LIB_MODULES),$(LATTE_LIB_WORKSPACE)/src/$(module)/lib.mk)
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/dict/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/utils/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/ae/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/anet/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/time/lib.mk