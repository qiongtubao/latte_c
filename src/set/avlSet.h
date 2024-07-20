#ifndef __LATTE_AVL_SET_H
#define __LATTE_AVL_SET_H
#include "stdlib.h"
#include "set.h"
#include "tree/avlTree.h"

typedef struct avlTree avlSet;
#define  avlSetCreate avlTreeCreate

int avlSetAdd(avlSet* set, void* key);
#define  avlSetRemove avlTreeRemove
#define avlSetRelease avlTreeRelease
#define  avlSetSize avlTreeSize
#define  avlSetGetAvlSetIterator avlTreeGetAvlTreeIterator
#define avlSetGetIterator avlTreeGetIterator
typedef struct avlTreeIterator avlSetIterator;
#define avlSetIteratorHasNext avlTreeIteratorHasNext
#define avlSetIteratorNext avlTreeIteratorNext
#define avlSetIteratorRelease avlTreeIteratorRelease
int avlSetContains(avlSet* set, void* element);

extern avlTreeType avlSetSdsType;
extern setType avlSetApi;
set* setCreateAvl(avlTreeType* type);


#endif



