ifneq ($(USED_TREE), yes) 
include $(WORKSPACE)/src/zmalloc/lib.mk
FINAL_CC_CFLAGS+= -I../tree

LIB_OBJ+= ../tree/avlTree.o
USED_TREE=yes
../tree/avlTree.o:
	cd ../tree && make avlTree.o

endif