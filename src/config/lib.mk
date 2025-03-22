

ifneq ($(USED_CONFIG), yes) 
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
FINAL_CC_CFLAGS+= -I../config 

../config/config.o:
	cd $(WORKSPACE)/src/config && make config.o

LIB_OBJ+= ../config/config.o 
USED_CONFIG=yes
endif