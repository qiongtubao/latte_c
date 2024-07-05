
ifneq ($(USED_DICT_PLUGINS), yes) 
FINAL_CC_CFLAGS+= -I../dict_plugins 


../dict_plugins/dict_plugins.o:
	cd $(WORKSPACE)/src/dict_plugins && make dict_plugins.o



LIB_OBJ+= ../dict_plugins/dict_plugins.o 
USED_DICT_PLUGINS=yes
endif