LATTE_LIB_WORKSPACE?=$(CURDIR)/../..
BUILD_OBJ?=latte_json.o 
LIB_OBJ?=
TEST_MAIN?=latte_json_test
LIB_MODULES?=zmalloc sds iterator dict vector value utils log
MODULE?=json
BUILD_INCLUDE=latte_json.h 

include $(LATTE_LIB_WORKSPACE)/mks/sys.mk
include $(LATTE_LIB_WORKSPACE)/mks/final.mk
include $(LATTE_LIB_WORKSPACE)/mks/malloc.mk
include $(LATTE_LIB_WORKSPACE)/mks/latte.mk
include $(LATTE_LIB_WORKSPACE)/mks/info.mk
include $(LATTE_LIB_WORKSPACE)/src/value/lib.mk

# FINAL_CC_CFLAGS+= -DUSE_JSONC -I../../deps/json-c/ -I../../deps/json-c/build
# FINAL_CXX_CFLAGS+= -DUSE_JSONC -I../../deps/json-c/ -I../../deps/json-c/build
# FINAL_CC_LIBS := ../../deps/json-c/build/libjson-c.a $(FINAL_CC_LIBS)
# FINAL_CXX_LIBS := ../../deps/json-c/build/libjson-c.a $(FINAL_CXX_LIBS)
