WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?= file.o dir.o env.o posix_file.o
LIB_OBJ?=
TEST_MAIN?=fs_test
LIB_MODULES?=zmalloc utils sds set
MODULE?=fs
BUILD_INCLUDE= file.h dir.h env.h posix_file.h flags.h

include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk
include $(WORKSPACE)/mks/latte.mk
include $(WORKSPACE)/mks/info.mk

include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
include $(WORKSPACE)/src/set/lib.mk




