ifneq ($(USED_VECTOR), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/log/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/iterator/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/cmp/lib.mk
FINAL_CC_CFLAGS+= -I../vector

LIB_OBJ+= ../vector/vector.o 
USED_VECTOR=yes
../vector/vector.o:
		cd ../vector && make vector.o


endif