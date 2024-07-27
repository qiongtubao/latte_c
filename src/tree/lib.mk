ifneq ($(USED_TREE), yes) 
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/iterator/lib.mk
FINAL_CC_CFLAGS+= -I../tree

LIB_OBJ+= ../tree/avlTree.o ../tree/rbTree.o
USED_TREE=yes
../tree/avlTree.o:
	cd ../tree && make avlTree.o

../tree/rbTree.o:
	cd ../tree && make rbTree.o
endif