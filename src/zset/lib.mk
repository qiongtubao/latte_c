
ifneq ($(USED_ZSET), yes) 
FINAL_CC_CFLAGS+= -I../zset
LIB_OBJ+= ../zset/zset.o
USED_ZSET=yes
../zset/zset.o:
		cd ../zset && make zset.o
endif

