ifneq ($(USED_FS), yes) 
include $(WORKSPACE)/src/set/lib.mk
FINAL_CC_CFLAGS+= -I../fs

LIB_OBJ+= ../fs/file.o ../fs/dir.o ../fs/env.o
USED_FS=yes
../fs/file.o:
	cd ../fs && make file.o

../fs/dir.o:
	cd ../fs && make dir.o

../fs/env.o:
	cd ../fs && make env.o
endif