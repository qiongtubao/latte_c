ifneq ($(USED_OBJECT), yes) 
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/ae/lib.mk
FINAL_CC_CFLAGS+= -I../object
LIB_OBJ+= ../object/object.o ../object/object_string.o ../object/object_set.o ../object/object_zset.o ../object/object_list.o ../object/object_module.o 
USED_OBJECT=yes
../object/object.o:
		cd ../object && make object.o
../object/object_string.o:
		cd ../object && make object_string.o
../object/object_set.o:
	cd ../object && make object_set.o
../object/object_zset.o:
	cd ../object && make object_zset.o
../object/object_list.o:
	cd ../object && make object_list.o
../object/object_module.o:
	cd ../object && make object_module.o
../object/object_stream.o:
	cd ../object && make object_stream.o
endif
