WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=io.o io_fd.o io_file.o io_conn.o io_buffer.o
LIB_OBJ?= 
TEST_MAIN?=io_test
MODULE?=io
LIB_MODULES?=zmalloc utils sds crc
BUILD_INCLUDE=io.h

include $(WORKSPACE)/mks/sys.mk
include $(WORKSPACE)/mks/final.mk
include $(WORKSPACE)/mks/malloc.mk

include $(WORKSPACE)/mks/info.mk

include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/log/lib.mk
include $(WORKSPACE)/src/crc/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
include $(WORKSPACE)/mks/latte.mk
