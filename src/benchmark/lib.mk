ifneq ($(BENCHMARK_TASK), yes) 
include $(LATTE_LIB_WORKSPACE)/src/zmalloc/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/list/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/ae/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/list/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/rax/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/sds/lib.mk
include $(LATTE_LIB_WORKSPACE)/src/endianconv/lib.mk
FINAL_CC_CFLAGS+= -I../benchmark
LIB_OBJ+= ../benchmark/benchmark.o 
BENCHMARK_TASK=yes
../benchmark/benchmark.o:
		cd ../benchmark && make benchmark.o

endif

