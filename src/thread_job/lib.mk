ifneq ($(USED_THREAD_JOB), yes) 
include $(LATTE_LIB_WORKSPACE)/src/thread_pool/lib.mk

FINAL_CC_CFLAGS+= -I../thread_job

LIB_OBJ+= ../thread_job/thread_job.o 
USED_THREAD_JOB=yes
../thread_job/thread_job.o:
		cd ../thread_job && make thread_job.o


endif