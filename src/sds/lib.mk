

FINAL_CC_CFLAGS+= -I../sds -I../zmalloc
LIB_OBJ+=../zmalloc/zmalloc.o
include $(WORKSPACE)/src/zmalloc/lib.mk

../sds/sds.o: ../zmalloc/zmalloc.o
	cd ../sds && make sds.o