

ifneq ($(USED_CUCKOO), yes) 
include $(LATTE_LIB_WORKSPACE)/src/siphash/lib.mk
FINAL_CC_CFLAGS+= -I../cuckoo
../cuckoo/cuckoo.o:
	cd ../cuckoo && make cuckoo.o


LIB_OBJ+= ../cuckoo.o 
USED_CUCKOO=yes
endif