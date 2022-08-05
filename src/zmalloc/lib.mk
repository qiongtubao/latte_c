

FINAL_CC_CFLAGS+= -I../zmalloc


../zmalloc/zmalloc.o:
	cd ../zmalloc && make zmalloc.o

LIB_OBJ+= ../zmalloc/zmalloc.o