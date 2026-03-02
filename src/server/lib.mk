ifneq ($(USED_SERVER), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/list/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/ae/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/list/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/connection/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/rax/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/endianconv/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/log/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/value/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/async_io/lib.mk
FINAL_CC_CFLAGS+= -I../server
LIB_OBJ+= ../server/server.o ../server/client.o ../server/cron.o
USED_SERVER=yes
../server/server.o:
		cd ../server && make server.o

../server/client.o:
		cd ../server && make client.o

../server/cron.o:
		cd ../server && make cron.o
endif

