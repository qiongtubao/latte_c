ifneq ($(USED_RAX), yes) 
FINAL_CC_CFLAGS+= -I../rax
../rax/rax.o:
	cd ../rax && make rax.o
LIB_OBJ+= ../rax/rax.o
USED_RAX=yes
endif