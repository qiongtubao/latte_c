

ifneq ($(USED_CONFIG), yes) 
include $(LATTE_LIB_WORKSPACE)/src/dict/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/utils/lib.mk
FINAL_CC_CFLAGS+= -I../config 

../config/config.o:
	cd $(LATTE_LIB_WORKSPACE)/src/config && make config.o

LIB_OBJ+= ../config/config.o 
USED_CONFIG=yes
endif