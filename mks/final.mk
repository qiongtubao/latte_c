
#WARN
WARN=-Wall -W -Wno-missing-field-initializers

#OPT
OPTIMIZATION?=-O2
OPT=$(OPTIMIZATION)

#DEBUG 
DEBUG=-g 
ifneq ($(uname_S), SunOS) 
	DEBUG+= -ggdb
endif

#CFLAGS
## 1. -D__ATOMIC_VAR_FORCE_SYNC_MACROS
## 2. -m32


#LATTE_CFLAGS
## 1. -I/usr/local/include
## 2. -fprofile-arcs -ftest-coverage -DCOVERAGE_TEST


#  init CC_STD
CC_STD=-pedantic -DREDIS_STATIC=''
C++_STD=-pedantic -DREDIS_STATIC=''

# Use -Wno-c11-extensions on clang, either where explicitly used or on
# platforms we can assume it's being used.
ifneq (,$(findstring clang,$(CC)))
  CC_STD+=-Wno-c11-extensions
  C++_STD+=-Wno-c11-extensions
else
ifneq (,$(findstring FreeBSD,$(uname_S)))
  CC_STD+=-Wno-c11-extensions
  C++_STD+=-Wno-c11-extensions
endif
endif

# Detect if the compiler supports C11 _Atomic
C11_ATOMIC := $(shell sh -c 'echo "\#include <stdatomic.h>" > foo.c; \
	$(CC) -std=c11 -c foo.c -o foo.o > /dev/null 2>&1; \
	if [ -f foo.o ]; then echo "yes"; rm foo.o; fi; rm foo.c')
ifeq ($(C11_ATOMIC),yes)
	CC_STD+=-std=c11
    C++_STD+=-std=c++11
else
	CC_STD+=-std=c99
    C++_STD+=-std=c++99
endif

# sys diff
include $(WORKSPACE)/mks/uname_s_final/$(uname_S).mk
include $(WORKSPACE)/mks/uname_m_final/$(uname_M).mk

#FINAL_CC_CFLAGS
FINAL_CC_CFLAGS=$(STD) $(WARN) $(OPT) $(DEBUG) $(CFLAGS) $(LATTE_CFLAGS)
#FINAL_CC_LIBS
FINAL_CC_LIBS=-I../
