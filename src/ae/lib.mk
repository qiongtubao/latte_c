ifneq ($(USED_AE), yes) 
include $(WORKSPACE)/src/anet/lib.mk
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/log/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/func_task/lib.mk

FINAL_CC_CFLAGS+= -I../ae
LIB_OBJ+= ../ae/ae.o ../ae/monotonic.o
USED_AE=yes
../ae/ae.o:
	cd ../ae && make ae.o
	
../ae/monotonic.o:
	cd ../ae && make monotonic.o
endif