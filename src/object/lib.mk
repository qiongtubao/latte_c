ifneq ($(USED_OBJECT), yes) 
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/ae/lib.mk
FINAL_CC_CFLAGS+= -I../object
LIB_OBJ+= ../object/object.o ../object/string.o
USED_OBJECT=yes
../object/object.o:
		cd ../object && make object.o
../object/string.o:
		cd ../object && make string.o
endif
