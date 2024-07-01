ifneq ($(USED_LOG), yes)
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/utils/lib.mk
FINAL_CC_CFLAGS+= -I../log
../log/log.o:
	cd ../log && make log.o
LIB_OBJ+= ../log/log.o
USED_LOG=yes
endif
