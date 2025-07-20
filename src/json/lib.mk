
ifneq ($(USED_JSON), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/iterator/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/vector/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/dict/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/value/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/utils/lib.mk
FINAL_CC_CFLAGS+= -I../json
../json/json.o:
	cd ../json && make json.o

	
LIB_OBJ+= ../json/json.o 
USED_JSON=yes
endif
