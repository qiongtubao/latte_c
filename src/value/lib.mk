
ifneq ($(USED_VALUE), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/iterator/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/dict/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/vector/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/log/lib.mk
FINAL_CC_CFLAGS+= -I../value
../value/value.o:
	cd ../value && make value.o
LIB_OBJ+= ../value/value.o
USED_VALUE=yes
endif
