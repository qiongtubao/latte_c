ifneq ($(USED_ERROR), yes) 
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/set/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/log/lib.mk
FINAL_CC_CFLAGS+= -I../error


../error/error.o:
	cd $(WORKSPACE)/src/error && make error.o



LIB_OBJ+= ../error/error.o
USED_FS=yes
endif