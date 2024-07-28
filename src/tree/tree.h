#ifndef __LATTE_TREE_H
#define __LATTE_TREE_H

#include "stdlib.h"
typedef struct treeType {
    int (*operator)(void* f1, void* f2);
    void (*releaseNode)(void* node);
    void* (*createNode)(void* key, void* value);
    void (*setVal)(void* node, void* value);
    void* (*getVal)(void* node);
} treeType;

#endif