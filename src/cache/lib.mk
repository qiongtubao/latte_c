

ifneq ($(USED_CACHE), yes) 
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
include $(WORKSPACE)/src/list/lib.mk
FINAL_CC_CFLAGS+= -I../config 

../cache/lru_cache.o:
	cd $(WORKSPACE)/src/config && make config.o

LIB_OBJ+= ../cache/lru_cache.o 
USED_CACHE=yes
endif