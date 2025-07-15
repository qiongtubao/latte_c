

ifneq ($(USED_LIST), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/iterator/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/lzf/lib.mk
FINAL_CC_CFLAGS+= -I../list
../list/list.o:
	cd ../list && make list.o

../list/listpack.o:
	cd ../list && make listpack.o
	
../list/quicklist.o:
	cd ../list && make quicklist.o

../list/ziplist.o:
	cd ../list && make ziplist.o

LIB_OBJ+= ../list/list.o ../list/listpack.o ../list/quicklist.o ../list/ziplist.o
USED_LIST=yes
endif
