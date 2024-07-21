ifneq ($(USED_ITERATOR), yes) 

FINAL_CC_CFLAGS+= -I../iterator

LIB_OBJ+= ../iterator/iterator.o
USED_ITERATOR=yes

../iterator/iterator.o:
	cd ../iterator && make iterator
endif