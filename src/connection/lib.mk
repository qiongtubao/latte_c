ifneq ($(USED_CONNECTION), yes) 
FINAL_CC_CFLAGS+= -I../connection 

../connection/connection.o:
	cd $(WORKSPACE)/src/connection && make connection.o

LIB_OBJ+= ../connection/connection.o 
USED_CONFIG=yes
endif