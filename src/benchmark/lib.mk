ifneq ($(BENCHMARK_TASK), yes) 
include $(WORKSPACE)/src/zmalloc/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/ae/lib.mk
include $(WORKSPACE)/src/list/lib.mk
include $(WORKSPACE)/src/rax/lib.mk
include $(WORKSPACE)/src/sds/lib.mk
include $(WORKSPACE)/src/endianconv/lib.mk
FINAL_CC_CFLAGS+= -I../benchmark
LIB_OBJ+= ../benchmark/benchmark.o 
BENCHMARK_TASK=yes
../benchmark/benchmark.o:
		cd ../benchmark && make benchmark.o

endif

