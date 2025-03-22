#ifndef __LATTE_DIR_H
#define __LATTE_DIR_H

#include "utils/error.h"
#include "iterator/iterator.h"

Error* dirCreate(char* path);
Error* dirCreateRecursive(const char* path, mode_t mode);
int dirIs(char* path);
latte_iterator_t* dir_scan_file(char* dir_path, const char* filter_pattern);

#endif