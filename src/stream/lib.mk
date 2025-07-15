


ifneq ($(USED_STREAM), yes) 
include $(LATTE_LIB_WORKSPACE)/src/rax/lib.mk
FINAL_CC_CFLAGS+= -I../stream
LIB_OBJ+= ../stream/stream.o
USED_STREAM=yes
../stream/stream.o:
		cd ../stream && make stream.o
endif

