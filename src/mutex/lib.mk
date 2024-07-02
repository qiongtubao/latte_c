ifneq ($(USED_MUTEX), yes)
FINAL_CC_CFLAGS+= -I../mutex

LIB_OBJ+= ../mutex/mutex.o
USED_MUTEX=yes
../mutex/mutex.o:
	cd ../mutex && make mutex.o

endif