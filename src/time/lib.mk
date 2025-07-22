
ifneq ($(USED_LOCALTIME), yes) 

FINAL_CC_CFLAGS+= -I../time
../time/localtime.o:
	cd ../time && make localtime.o
LIB_OBJ+= ../time/localtime.o
USED_LOCALTIME=yes
endif