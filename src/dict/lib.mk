
ifneq ($(USED_DICT), yes) 
FINAL_CC_CFLAGS+= -I../dict 


../dict/dict.o:
	cd $(WORKSPACE)/src/dict && make dict.o

../dict/siphash.o:
	cd $(WORKSPACE)/src/dict && make siphash.o


LIB_OBJ+= ../dict/dict.o ../dict/siphash.o
USED_DICT=yes
endif