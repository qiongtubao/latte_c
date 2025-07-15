ifneq ($(USED_SET), yes) 
include $(LATTE_LIB_WORKSPACE)/src/dict/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/mutex/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/tree/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/iterator/lib.mk
FINAL_CC_CFLAGS+= -I../set



	
../set/avl_set.o:
	cd $(LATTE_LIB_WORKSPACE)/src/set && make avl_set.o

../set/hash_set.o:
	cd $(LATTE_LIB_WORKSPACE)/src/set && make hash_set.o

../set/set.o:
	cd $(LATTE_LIB_WORKSPACE)/src/set && make set.o

../set/int_set.o:
	cd $(LATTE_LIB_WORKSPACE)/src/set && make int_set.o

LIB_OBJ+=  ../set/avl_set.o ../set/hash_set.o ../set/set.o ../set/int_set.o

USED_SET=yes
endif