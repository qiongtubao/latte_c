LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=server.o client.o cron.o
LIB_OBJ?=
TEST_MAIN?=server_test
LIB_MODULES?=zmalloc sds list endianconv connection ae rax log value func_task
MODULE?=server
BUILD_INCLUDE=server.h client.h cron.h

include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk
include $(LATTE_LIB_WORKSPACE)/mks/sys_config.mk
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/list/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/ae/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/connection/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/rax/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/endianconv/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/log/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/value/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/func_task/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/async_io/lib.mk