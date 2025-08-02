ifneq ($(ASYNC_IO_TASK), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
FINAL_CC_CFLAGS+= -I../async_io
LIB_OBJ+= ../async_io/async_io.o 
ASYNC_IO_TASK=yes
../async_io/async_io.o:
		cd ../async_io && make async_io.o

endif
