FINAL_CC_CFLAGS+= -I../sds

LIB_OBJ+= ../sds/sds.o
../sds/sds.o:
	cd ../sds && make sds.o