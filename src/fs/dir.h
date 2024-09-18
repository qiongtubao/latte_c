#ifndef __LATTE_DIR_H
#define __LATTE_DIR_H

#include "utils/error.h"

Error* dirCreate(char* path);
Error* dirCreateRecursive(const char* path, mode_t mode);

#endif