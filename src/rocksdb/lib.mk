ifneq ($(USED_ROCKSDB), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/mks/rocksdb.mk
FINAL_CC_CFLAGS+= -I../rocksdb 

LIB_OBJ+= ../rocksdb/latte_rocksdb.o 
USED_ROCKSDB=yes

../rocksdb/latte_rocksdb.o:
	cd ../rocksdb && make latte_rocksdb.o

endif