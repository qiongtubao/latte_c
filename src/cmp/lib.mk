

ifneq ($(USED_CMP), yes) 
FINAL_CC_CFLAGS+= -I../cmp 

../cmp/cmp.o :
	cd $(WORKSPACE)/src/cmp && make cmp.o

LIB_OBJ+= ../cmp/cmp.o 
USED_CMP=yes
endif