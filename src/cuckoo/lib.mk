

ifneq ($(USED_CUCKOO), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/siphash/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/debug/lib.mk
FINAL_CC_CFLAGS+= -I../cuckoo
../cuckoo/cuckoo.o:
	cd ../cuckoo && make cuckoo.o


LIB_OBJ+= ../cuckoo/cuckoo.o
USED_CUCKOO=yes
endif
