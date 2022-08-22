ifneq ($(USED_TASK), yes) 
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/ae/lib.mk
FINAL_CC_CFLAGS+= -I../task
LIB_OBJ+= ../task/task.o ../task/setcpuaffinity.o
USED_TASK=yes
../task/task.o:
		cd ../task && make task.o
../task/setcpuaffinity.o:
		cd ../task && make setcpuaffinity.o
endif

