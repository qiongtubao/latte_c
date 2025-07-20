ifneq ($(USED_ERROR), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/set/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/utils/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/list/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/log/lib.mk
FINAL_CC_CFLAGS+= -I../error


../error/error.o:
	cd $(LATTE_LIB_WORKSPACE)/src/error && make error.o



LIB_OBJ+= ../error/error.o
USED_ERROR=yes
endif