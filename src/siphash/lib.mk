ifneq ($(USED_SIPHASH), yes) 

FINAL_CC_CFLAGS+= -I../siphash
LIB_OBJ+= ../siphash/siphash.o
USED_SIPHASH=yes
../siphash/siphash.o:
	cd ../siphash && make siphash.o
endif
