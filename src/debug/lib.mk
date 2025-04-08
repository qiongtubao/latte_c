
ifneq ($(USED_DEBUG), yes) 
include $(WORKSPACE)/src/log/lib.mk
FINAL_CC_CFLAGS+= -I../latte_debug 


../debug/latte_debug.o:
	cd $(WORKSPACE)/src/debug && make latte_debug.o


LIB_OBJ+= ../debug/latte_debug.o
USED_DEBUG=yes
endif