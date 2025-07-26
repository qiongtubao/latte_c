ifneq ($(USED_THREAD_POOL), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/log/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/vector/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/utils/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/time/lib.mk
FINAL_CC_CFLAGS+= -I../thread_pool

LIB_OBJ+= ../thread_pool/thread_pool.o 
USED_THREAD_POOL=yes
../thread_pool/thread_pool.o:
		cd ../thread_pool && make thread_pool.o


endif