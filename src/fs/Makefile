LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?= file.o dir.o env.o posix_file.o fs.o
LIB_OBJ?=
TEST_MAIN?=fs_test
LIB_MODULES?=zmalloc utils sds set log list
MODULE?=fs
BUILD_INCLUDE= file.h dir.h env.h posix_file.h flags.h fs.h

include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk

include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/utils/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/set/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/list/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/log/lib.mk



