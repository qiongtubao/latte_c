ifneq ($(USED_FS), yes) 
FINAL_CC_CFLAGS+= -I../fs

LIB_OBJ+= ../fs/file.o ../dir/dir.o
USED_FS=yes
../fs/file.o:
	cd ../fs/file && make file.o

../fs/dir.o:
	cd ../fs/dir && make dir.o
endif