

ifneq ($(USED_CONFIG), yes) 
FINAL_CC_CFLAGS+= -I../config 

../config/config.o:
	cd $(WORKSPACE)/src/config && make config.o

LIB_OBJ+= ../config/config.o 
USED_CONFIG=yes
endif