#ifndef LATTE_CRC_H
#define LATTE_CRC_H
#include <inttypes.h>
#include <stdio.h>
//#if !defined(HAVE_FDATASYNC)：这是一个预处理器条件指令，它检查宏HAVE_FDATASYNC是否已经被定义。如果没有被定义，那么预处理器将执行下一个指令。
//#cmakedefine01 HAVE_FDATASYNC：这是CMake提供的一种特殊的预处理器指令，用于在源代码中定义宏。如果fdatasync函数在目标平台上可用，CMake会将这个宏定义为1；否则，定义为0。这使得源代码可以根据编译时的平台特性来调整自身的行为。
//#endif  // !defined(HAVE_FDATASYNC)：这是条件指令的结束标记，它告诉预处理器结束对#if指令的条件检查。
// Define to 1 if you have Google CRC32C.
// #if !defined(HAVE_CRC32C)
// #cmakedefine01 HAVE_CRC32C
// #endif  // !defined(HAVE_CRC32C)
//crc16 
uint16_t crc16xmodem(const char *buf, int len);

//crc32
uint32_t crc32c(const char* buf, int len);
uint32_t crc32c_extend(uint32_t crc, const char* buf, int len);
uint32_t crc32c_mask(uint32_t);
uint32_t crc32c_unmask(uint32_t masked_crc);


//crc32-jamcrc算法
uint32_t crc32jamcrc(const char *buffer, int len);




// crc64
uint64_t crc64_redis(uint64_t crc, const unsigned char *s, uint64_t l);
#endif
