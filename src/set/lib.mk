ifneq ($(USED_SET), yes) 
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/mutex/lib.mk
include $(WORKSPACE)/src/tree/lib.mk
include $(WORKSPACE)/src/iterator/lib.mk
FINAL_CC_CFLAGS+= -I../set



	
../set/avl_set.o:
	cd $(WORKSPACE)/src/set && make avl_set.o

../set/hash_set.o:
	cd $(WORKSPACE)/src/set && make hash_set.o

../set/set.o:
	cd $(WORKSPACE)/src/set && make set.o

LIB_OBJ+=  ../set/avl_set.o ../set/hash_set.o ../set/set.o

USED_SET=yes
endif