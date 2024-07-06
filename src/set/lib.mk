ifneq ($(USED_SET), yes) 
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/dict_plugins/lib.mk
include $(WORKSPACE)/src/mutex/lib.mk
FINAL_CC_CFLAGS+= -I../set

LIB_OBJ+= ../set/set.o ../set/lockSet.o
USED_SET=yes
../set/set.o:
	cd ../set && make set.o
../set/lockSet.o:
	cd ../set && make lockSet.o
endif