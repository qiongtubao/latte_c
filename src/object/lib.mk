ifneq ($(USED_OBJECT), yes)
FINAL_CC_CFLAGS+= -I../object

LIB_OBJ+= ../object/object_manager.o ../object/object.o
USED_OBJECT=yes

../object/object_manager.o ../object/object.o:
	cd $(LATTE_LIB_WORKSPACE)/src/object && make object_manager.o object.o
endif
