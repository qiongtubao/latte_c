
# debug is a dependency-free leaf module: it must be linkable from the lowest
# layers of the library, so do NOT add module includes here.
ifneq ($(USED_DEBUG), yes)
FINAL_CC_CFLAGS+= -I../debug


../debug/latte_debug.o:
	cd $(LATTE_LIB_WORKSPACE)/src/debug && make latte_debug.o


LIB_OBJ+= ../debug/latte_debug.o
USED_DEBUG=yes
endif
