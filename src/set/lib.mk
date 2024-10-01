ifneq ($(USED_SET), yes) 
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/mutex/lib.mk
include $(WORKSPACE)/src/tree/lib.mk
include $(WORKSPACE)/src/iterator/lib.mk
FINAL_CC_CFLAGS+= -I../set



../set/lockSet.o:
	cd $(WORKSPACE)/src/set && make lockSet.o
	
../set/avlSet.o:
	cd $(WORKSPACE)/src/set && make avlSet.o

../set/hashSet.o:
	cd $(WORKSPACE)/src/set && make hashSet.o

../set/set.o:
	cd $(WORKSPACE)/src/set && make set.o

LIB_OBJ+=  ../set/lockSet.o ../set/avlSet.o ../set/hashSet.o ../set/set.o

USED_SET=yes
endif