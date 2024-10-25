
ifneq ($(USED_CRC), yes) 
include $(WORKSPACE)/src/sds/lib.mk
FINAL_CC_CFLAGS+= -I../crc 

../crc/crc16xmodem.o:
	cd $(WORKSPACE)/src/crc && make crc16xmodem.o

../crc/crc32c.o:
	cd $(WORKSPACE)/src/crc && make crc32c.o

LIB_OBJ+= ../crc/crc16xmodem.o ../crc/crc32c.o
USED_CRC=yes
endif