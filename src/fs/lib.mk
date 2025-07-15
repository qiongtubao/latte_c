ifneq ($(USED_FS), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/set/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/utils/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/list/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/log/lib.mk
FINAL_CC_CFLAGS+= -I../fs


../fs/file.o:
	cd $(LATTE_LIB_WORKSPACE)/src/fs && make file.o

../fs/dir.o:
	cd $(LATTE_LIB_WORKSPACE)/src/fs && make dir.o

../fs/env.o:
	cd $(LATTE_LIB_WORKSPACE)/src/fs && make env.o

../fs/posix_file.o:
	cd $(LATTE_LIB_WORKSPACE)/src/fs && make posix_file.o

../fs/fs.o:
	cd $(LATTE_LIB_WORKSPACE)/src/fs && make fs.o

LIB_OBJ+= ../fs/file.o ../fs/dir.o ../fs/env.o ../fs/posix_file.o ../fs/fs.o
USED_FS=yes
endif