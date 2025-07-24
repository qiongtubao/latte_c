ifneq ($(USED_THREAD_SINGLE_OBJECT), yes) 
FINAL_CC_CFLAGS+= -I../zset
FINAL_CC_CFLAGS+= -I../thread_single_object
LIB_OBJ+= ../thread_single_object/thread_single_object.o
USED_THREAD_SINGLE_OBJECT=yes
../thread_single_object/thread_single_object.o:
		cd ../thread_single_object && make thread_single_object.o
endif

