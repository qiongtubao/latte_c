ifneq ($(USED_UTILS), yes) 
FINAL_CC_CFLAGS+= -I../utils
LIB_OBJ+= ../utils/utils.o ../utils/error.o
USED_UTILS=yes
../utils/utils.o:
		cd ../utils && make utils.o

../error/error.o:
		cd ../utils && make error.o
endif