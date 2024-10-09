ifneq ($(USED_TREE), yes) 
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
include $(WORKSPACE)/src/log/lib.mk
FINAL_CC_CFLAGS+= -I../tree


USED_TREE=yes
../tree/avlTree.o:
	cd ../tree && make avlTree.o

../tree/bPlusTree.o:
	cd ../tree && make bPlusTree.o

LIB_OBJ+= ../tree/avlTree.o ../tree/bPlusTree.o

endif