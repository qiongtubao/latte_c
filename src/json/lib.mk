
ifneq ($(USED_JSON), yes) 
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/iterator/lib.mk
include $(WORKSPACE)/src/vector/lib.mk
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/value/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
FINAL_CC_CFLAGS+= -I../json
../json/json.o:
	cd ../json && make json.o

	
LIB_OBJ+= ../json/json.o 
USED_JSON=yes
endif
