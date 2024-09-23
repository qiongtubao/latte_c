

ifneq ($(USED_LIST), yes) 
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/iterator/lib.mk
FINAL_CC_CFLAGS+= -I../list
../list/list.o:
	cd ../list && make list.o
LIB_OBJ+= ../list/list.o
USED_LIST=yes
endif
