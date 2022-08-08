ifneq ($(USED_ZMALLOC), yes) 
FINAL_CC_CFLAGS+= -I../zmalloc
LIB_OBJ+= ../zmalloc/zmalloc.o
USED_ZMALLOC=yes
../zmalloc/zmalloc.o:
		cd ../zmalloc && make zmalloc.o
endif

