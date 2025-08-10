ifneq ($(USED_AE), yes) 
include $(LATTE_LIB_WORKSPACE)/src/anet/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/dict/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/log/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/utils/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/list/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/func_task/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/debug/lib.mk
ifdef IOURING
    include $(LATTE_LIB_WORKSPACE)/mks/sys_config.mk
endif
FINAL_CC_CFLAGS+= -I../ae
LIB_OBJ+= ../ae/ae.o 
USED_AE=yes
../ae/ae.o:
	cd ../ae && make ae.o
	
endif