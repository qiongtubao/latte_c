ifneq ($(USED_FUNC_TASK), yes) 
include $(WORKSPACE)/src/dict/lib.mk
include $(WORKSPACE)/src/log/lib.mk
FINAL_CC_CFLAGS+= -I../func_task
LIB_OBJ+= ../func_task/func_task.o 
USED_FUNC_TASK=yes
../func_task/func_task.o:
	cd ../func_task && make func_task.o
	
endif