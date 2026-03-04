ifneq ($(USED_ODB), yes)
FINAL_CC_CFLAGS+= -I../odb

LIB_OBJ+= ../odb/odb.o
USED_ODB=yes


../odb/odb.o:
	cd ../odb && make odb.o
endif