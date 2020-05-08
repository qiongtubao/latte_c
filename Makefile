src := $(shell ls *.c) 
objs := $(patsubst %.c,%.o,$(src))  
STD=-std=c99 -pedantic -DREDIS_STATIC=''
WARN=-Wall -W -Wno-missing-field-initializers
OPT=$(OPTIMIZATION)
FINAL_CFLAGS=$(STD) $(WARN) $(OPT) $(DEBUG) $(CFLAGS) 
FINAL_LDFLAGS=$(LDFLAGS)  $(DEBUG)
FINAL_LIBS=-lm
DEBUG=-g -ggdb
MALLOC=libc
ifeq ($(MALLOC),jemalloc)
	DEPENDENCY_TARGETS+= jemalloc
	FINAL_CFLAGS+= -DUSE_JEMALLOC -I./libs/jemalloc-4.0.3/include
	FINAL_LIBS+= ./libs/jemalloc-4.0.3/lib/libjemalloc.a
endif
LATTE_CC=$(QUIET_CC)$(CC) $(FINAL_CFLAGS)
LATTE_LD=$(QUIET_LINK)$(CC) $(FINAL_LDFLAGS)
CCCOLOR="\033[34m"
LINKCOLOR="\033[34;1m"
SRCCOLOR="\033[33m"
BINCOLOR="\033[37;1m"
MAKECOLOR="\033[32;1m"
ENDCOLOR="\033[0m"

ifndef V
QUIET_CC = @printf '    %b %b\n' $(CCCOLOR)CC$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_LINK = @printf '    %b %b\n' $(LINKCOLOR)LINK$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_INSTALL = @printf '    %b %b\n' $(LINKCOLOR)INSTALL$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;
endif

test: $(objs) zmalloc.o
	$(CC) $(FINAL_CFLAGS) -g main.c zmalloc.o -o main  
%.o: %.c .make-prerequisites
	$(LATTE_CC) -c $<
clean:
	rm -f  test *.o main


