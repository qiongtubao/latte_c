


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




# 1. make info USE_JEMALLOC=yes
# 2. make info USE_TCMALLOC_MINIMAL=yes
# 3. make info USE_TCMALLOC=yes
# 4. make info