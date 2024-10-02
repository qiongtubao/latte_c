
ifneq ($(USED_VALUE), yes) 
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/iterator/lib.mk
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/vector/lib.mk
FINAL_CC_CFLAGS+= -I../value
../value/value.o:
	cd ../value && make value.o
LIB_OBJ+= ../value/value.o
USED_VALUE=yes
endif
