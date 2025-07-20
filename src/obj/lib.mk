ifneq ($(USED_OBJ), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/utils/lib.mk
FINAL_CC_CFLAGS+= -I../obj
LIB_OBJ+= ../obj/obj.o
USED_OBJ=yes
../obj/obj.o:
		cd ../obj && make obj.o
endif

