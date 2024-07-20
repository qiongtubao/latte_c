ifneq ($(USED_ITERATOR), yes) 

FINAL_CC_CFLAGS+= -I../iterator

LIB_OBJ+= 
USED_ITERATOR=yes


endif