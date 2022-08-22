ifneq ($(USED_ANET), yes) 
FINAL_CC_CFLAGS+= -I../anet
LIB_OBJ+= ../anet/anet.o
USED_ANET=yes
../anet/anet.o:
		cd ../anet && make anet.o
endif

