#ifndef __REDIS_ASSERT_H__
#define __REDIS_ASSERT_H__

#include <unistd.h> /* for _exit() */

void _latteAssert(char *estr, char *file, int line);
void _lattePanic(const char *file, int line, const char *msg, ...);

#define assert(_e) ((_e)?(void)0 : (_latteAssert(#_e,__FILE__,__LINE__),_exit(1)))
#define panic(...) _lattePanic(__FILE__,__LINE__,__VA_ARGS__),_exit(1)



#endif
