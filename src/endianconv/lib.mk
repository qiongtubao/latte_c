
ifneq ($(USED_ENDIANCONV), yes) 
FINAL_CC_CFLAGS+= -I../endianconv 

../endianconv/endianconv.o:
	cd $(WORKSPACE)/src/endianconv && make endianconv.o

LIB_OBJ+= ../endianconv/endianconv.o 
USED_ENDIANCONV=yes
endif