#include <stdint.h>
uint64_t
_crc32 (uint64_t init, const unsigned char *buf, int len);
#ifdef CRC32_TEST_MAIN
int crc32Test(int argc, char *argv[]);
#endif
