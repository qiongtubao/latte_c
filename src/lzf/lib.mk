ifneq ($(USED_LZF), yes)
FINAL_CC_CFLAGS+= -I../lzf

LIB_OBJ+= ../lzf/lzf_d.o ../lzf/lzf_c.o
USED_LZF=yes
../lzf/lzf_c.o:
	cd ../lzf && make lzf_c.o

../lzf/lzf_d.o:
	cd ../lzf && make lzf_d.o

endif