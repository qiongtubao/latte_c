


# Default allocator defaults to Jemalloc if it's not an ARM
MALLOC=libc


ifneq ($(uname_M),armv6l)
ifneq ($(uname_M),armv7l)
ifeq ($(uname_S),Linux)
	MALLOC=jemalloc
endif
endif
endif

# Backwards compatibility for selecting an allocator
ifeq ($(USE_TCMALLOC),yes)
	MALLOC=tcmalloc
endif

ifeq ($(USE_TCMALLOC_MINIMAL),yes)
	MALLOC=tcmalloc_minimal
endif

ifeq ($(USE_JEMALLOC),yes)
	MALLOC=jemalloc
endif

ifeq ($(USE_JEMALLOC),no)
	MALLOC=libc
endif




# use MALLOC
ifeq ($(MALLOC),tcmalloc)
	FINAL_CC_CFLAGS+= -DUSE_TCMALLOC
	FINAL_CXX_CFLAGS+= -DUSE_TCMALLOC
	FINAL_CC_LIBS+= -ltcmalloc
	FINAL_CXX_LIBS+= -ltcmalloc_minimal
endif

ifeq ($(MALLOC),tcmalloc_minimal)
	FINAL_CC_CFLAGS+= -DUSE_TCMALLOC
	FINAL_CXX_CFLAGS+= -DUSE_TCMALLOC
	FINAL_CC_LIBS+= -ltcmalloc_minimal
	FINAL_CXX_LIBS+= -ltcmalloc_minimal
endif

ifeq ($(MALLOC),jemalloc)
	DEPENDENCY_TARGETS+= jemalloc
	FINAL_CC_CFLAGS+= -DUSE_JEMALLOC -I../../deps/jemalloc/include
	FINAL_CXX_CFLAGS+= -DUSE_JEMALLOC -I../../deps/jemalloc/include
	FINAL_CC_LIBS := ../../deps/jemalloc/lib/libjemalloc.a -lpthread -ldl $(FINAL_CC_LIBS)
	FINAL_CXX_LIBS := ../../deps/jemalloc/lib/libjemalloc.a $(FINAL_CXX_LIBS)
endif


# 1. make info USE_JEMALLOC=yes
# 2. make info USE_TCMALLOC_MINIMAL=yes
# 3. make info USE_TCMALLOC=yes
# 4. make info