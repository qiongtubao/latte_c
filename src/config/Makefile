LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=config.o
LIB_OBJ?=
TEST_MAIN?=config_test
MODULE?=config
BUILD_INCLUDE=config.h
LIB_MODULES?=zmalloc sds dict utils value
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
include $(LATTE_LIB_WORKSPACE)/src/value/lib.mk
