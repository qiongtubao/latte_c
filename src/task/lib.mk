ifneq ($(USED_TASK), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/list/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/ae/lib.mk
FINAL_CC_CFLAGS+= -I../task
LIB_OBJ+= ../task/task.o ../task/setcpuaffinity.o
USED_TASK=yes
../task/task.o:
		cd ../task && make task.o
../task/setcpuaffinity.o:
		cd ../task && make setcpuaffinity.o
endif

