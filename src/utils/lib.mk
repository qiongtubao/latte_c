ifneq ($(USED_UTILS), yes) 
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/sds/lib.mk

FINAL_CC_CFLAGS+= -I../utils

LIB_OBJ+= ../utils/utils.o ../utils/error.o ../utils/monotonic.o
USED_UTILS=yes
../utils/utils.o:
		cd ../utils && make utils.o

../error/error.o:
		cd ../utils && make error.o

../utils/monotonic.o:
		cd ../utils && make monotonic.o
endif