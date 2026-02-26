ifneq ($(USED_RDB), yes)
include $(LATTE_LIB_WORKSPACE)/src/object/lib.mk
FINAL_CC_CFLAGS+= -I../rdb
LIB_OBJ+= ../rdb/rdb.o
USED_RDB=yes
../rdb/rdb.o:
		cd ../rdb && make rdb.o
endif
