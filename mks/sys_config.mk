




ifneq ($(uname_M),armv6l)
ifneq ($(uname_M),armv7l)
ifeq ($(uname_S),Linux)
	FINAL_CC_CFLAGS+=-DHAVE_IOURING 
	FINAL_CC_LDFLAGS+=-DHAVE_IOURING 
	FINAL_CC_LIBS+=-luring
	FINAL_CXX_LIBS+=-luring
endif
endif
endif

