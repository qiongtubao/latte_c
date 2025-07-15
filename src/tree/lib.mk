ifneq ($(USED_TREE), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/utils/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/log/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/vector/lib.mk
FINAL_CC_CFLAGS+= -I../tree


USED_TREE=yes
../tree/avl_tree.o:
	cd ../tree && make avl_tree.o

../tree/b_plus_tree.o:
	cd ../tree && make b_plus_tree.o

LIB_OBJ+= ../tree/avl_tree.o ../tree/b_plus_tree.o

endif