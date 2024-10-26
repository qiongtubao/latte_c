ifneq ($(USED_SERVER), yes) 
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/ae/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/connection/lib.mk
include $(WORKSPACE)/src/rax/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/endianconv/lib.mk
include $(WORKSPACE)/src/log/lib.mk
include $(WORKSPACE)/src/value/lib.mk
FINAL_CC_CFLAGS+= -I../server
LIB_OBJ+= ../server/server.o 
USED_SERVER=yes
../server/server.o:
		cd ../server && make server.o

endif

