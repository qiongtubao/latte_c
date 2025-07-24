ifneq ($(USED_LOG), yes)
include $(LATTE_LIB_WORKSPACE)/src/dict/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/utils/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/time/lib.mk
FINAL_CC_CFLAGS+= -I../log
../log/log.o:
	cd ../log && make log.o
LIB_OBJ+= ../log/log.o
USED_LOG=yes
endif
