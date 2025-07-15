

ifneq ($(USED_CACHE), yes) 
include $(LATTE_LIB_WORKSPACE)/src/dict/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/utils/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/list/lib.mk
FINAL_CC_CFLAGS+= -I../config 

../cache/lru_cache.o:
	cd $(LATTE_LIB_WORKSPACE)/src/config && make config.o

LIB_OBJ+= ../cache/lru_cache.o 
USED_CACHE=yes
endif