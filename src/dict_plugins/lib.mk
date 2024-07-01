
ifneq ($(USED_DICT), yes) 
FINAL_CC_CFLAGS+= -I../dict 


../dict_plugins/dict_plugins.o:
	cd $(WORKSPACE)/src/dict_plugins && make dict_plugins.o



LIB_OBJ+= ../dict_plugins/dict_plugins.o 
USED_DICT=yes
endif