ifneq ($(USED_SET), yes) 
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/dict_plugins/lib.mk
include $(WORKSPACE)/src/mutex/lib.mk
include $(WORKSPACE)/src/tree/lib.mk
include $(WORKSPACE)/src/iterator/lib.mk
FINAL_CC_CFLAGS+= -I../set

LIB_OBJ+=  ../set/lockSet.o ../set/avlSet.o ../set/hashSet.o ../set/set.o:

USED_SET=yes

../set/lockSet.o:
	cd ../set && make lockSet.o
	
../set/avlSet.o:
	cd ../set && make avlSet.o

../set/hashSet.o:
	cd ../set && make hashSet.o

../set/set.o:
	cd ../set && make set.o
endif