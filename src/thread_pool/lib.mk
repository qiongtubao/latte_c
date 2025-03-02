ifneq ($(USED_THREAD_POOL), yes) 
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
include $(WORKSPACE)/src/log/lib.mk
include $(WORKSPACE)/src/vector/lib.mk
FINAL_CC_CFLAGS+= -I../thread_pool


USED_THREAD_POOL=yes
../thread_pool/thread_pool.o:
	cd ../thread_pool && make thread_pool.o


LIB_OBJ+= ../thread_pool/thread_pool.o 
endif