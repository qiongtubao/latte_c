LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?= crc32c.o crc16xmodem.o crc64.o crc64_redis.o 
LIB_OBJ?=
TEST_MAIN?=crc_test
LIB_MODULES?=sds
MODULE?=crc
BUILD_INCLUDE=crc.h crc64.h

include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk