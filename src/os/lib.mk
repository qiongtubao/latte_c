ifneq ($(USED_OS), yes)
FINAL_CC_CFLAGS+= -I../os

LIB_OBJ+= ../os/pidfile.o ../os/process.o ../os/signal.o
USED_OS=yes
../os/pidfile.o:
	cd ../os && make pidfile.o

../os/process.o:
	cd ../os && make process.o

../os/signal.o:
	cd ../os && make signal.o
endif