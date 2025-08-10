ifneq ($(USED_UTILS), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk

FINAL_CC_CFLAGS+= -I../utils

LIB_OBJ+= ../utils/utils.o ../utils/monotonic.o
USED_UTILS=yes
../utils/utils.o:
		cd ../utils && make utils.o


../utils/monotonic.o:
		cd ../utils && make monotonic.o
endif