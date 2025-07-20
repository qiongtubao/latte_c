ifneq ($(USED_SDS), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
FINAL_CC_CFLAGS+= -I../sds 

LIB_OBJ+= ../sds/sds.o ../sds/sds_plugins.o
USED_SDS=yes

../sds/sds.o:
	cd ../sds && make sds.o

../sds/sds_plugins.o:
	cd ../sds && make sds_plugins.o
endif