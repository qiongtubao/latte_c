
ifneq ($(USED_BITMAP), yes) 

include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/iterator/lib.mk
FINAL_CC_CFLAGS+= -I../bitmap 

../bitmap/bitmap.o:
	cd $(LATTE_LIB_WORKSPACE)/src/bitmap && make bitmap.o

LIB_OBJ+= ../bitmap/bitmap.o 
USED_CONFIG=yes
endif