

ifneq ($(USED_LIST), yes) 
FINAL_CC_CFLAGS+= -I../list
../list/list.o:
	cd ../list && make list.o
LIB_OBJ+= ../list/list.o
USED_LIST=yes
endif
