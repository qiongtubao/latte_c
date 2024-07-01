ifneq ($(USED_SDS), yes) 
FINAL_CC_CFLAGS+= -I../sds

LIB_OBJ+= ../sds/sds.o
USED_SDS=yes
../sds/sds.o:
	cd ../sds && make sds.o

endif