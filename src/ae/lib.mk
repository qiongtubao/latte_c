ifneq ($(USED_AE), yes) 
include $(WORKSPACE)/src/anet/anet.mk
FINAL_CC_CFLAGS+= -I../ae
LIB_OBJ+= ../ae/ae.o ../ae/monotonic.o
USED_AE=yes
../ae/ae.o:
	cd ../ae && make ae.o
	
../ae/monotonic.o:
	cd ../ae && make monotonic.o
endif