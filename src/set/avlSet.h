#ifndef __LATTE_ORDERLY_SET_H
#define __LATTE_ORDERLY_SET_H
#include "stdlib.h"
#include "set.h"
#include "tree/avlTree.h"

typedef struct avlTree avlSet;
#define  avlSetCreate avlTreeCreate
inline  int avlSetAdd(avlSet* set, void* key) {
    return avlTreePut(set, key, NULL);
}
#define  avlSetRemove avlTreeRemove
#define avlSetRelease avlTreeRelease
#define  avlSetSize avlTreeSize
#define  avlSetGetAvlSetIterator avlTreeGetAvlTreeIterator
#define avlSetGetIterator avlTreeGetIterator
typedef struct avlTreeIterator avlSetIterator;
#define avlSetIteratorHasNext avlTreeIteratorHasNext
#define avlSetIteratorNext avlTreeIteratorNext
#define avlSetIteratorRelease avlTreeIteratorRelease
inline int avlSetContains(avlSet* set, void* element) {
    avlNode* node = avlTreeGet(set, element);
    return node == NULL? 0 : 1;
}

extern avlTreeType avlSetSdsType;
set* setCreateAvl(avlTreeType* type);


#endif



