ifneq ($(USED_QUICKLIST), yes)
FINAL_CC_CFLAGS+= -I../quicklist

LIB_OBJ+= ../quicklist/quicklist.o
USED_QUICKLIST=yes
../quicklist/quicklist.o:
	cd ../quicklist && make quicklist.o

endif