
ifneq ($(USED_DICT), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/iterator/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/cmp/lib.mk
FINAL_CC_CFLAGS+= -I../dict 


../dict/dict.o:
	cd $(LATTE_LIB_WORKSPACE)/src/dict && make dict.o

../dict/siphash.o:
	cd $(LATTE_LIB_WORKSPACE)/src/dict && make siphash.o

../dict/dict_plugins.o:
	cd $(LATTE_LIB_WORKSPACE)/src/dict && make dict_plugins.o

LIB_OBJ+= ../dict/dict.o ../dict/siphash.o ../dict/dict_plugins.o
USED_DICT=yes
endif