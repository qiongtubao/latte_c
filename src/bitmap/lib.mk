
ifneq ($(USED_BITMAP), yes) 

include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/iterator/lib.mk
FINAL_CC_CFLAGS+= -I../bitmap 

../bitmap/bitmap.o:
	cd $(WORKSPACE)/src/bitmap && make bitmap.o

LIB_OBJ+= ../bitmap/bitmap.o 
USED_CONFIG=yes
endif