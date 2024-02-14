ifneq ($(USED_LOG), yes) 
FINAL_CC_CFLAGS+= -I../log
../log/log.o:
	cd ../list && make log.o
LIB_OBJ+= ../log/log.o
USED_LOG=yes
endif
