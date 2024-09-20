ifneq ($(USED_FS), yes) 
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/set/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
FINAL_CC_CFLAGS+= -I../fs


../fs/file.o:
	cd $(WORKSPACE)/src/fs && make file.o

../fs/dir.o:
	cd $(WORKSPACE)/src/fs && make dir.o

../fs/env.o:
	cd $(WORKSPACE)/src/fs && make env.o

../fs/posix_file.o:
	cd $(WORKSPACE)/src/fs && make posix_file.o

../fs/fs.o:
	cd $(WORKSPACE)/src/fs && make fs.o

LIB_OBJ+= ../fs/file.o ../fs/dir.o ../fs/env.o ../fs/posix_file.o ../fs/fs.o
USED_FS=yes
endif