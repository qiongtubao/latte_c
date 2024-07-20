
#ifndef __LATTE_HASH_SET_H
#define __LATTE_HASH_SET_H

#include <dict/dict.h>
#include <dict_plugins/dict_plugins.h>
#include "set.h"

typedef struct dict hashSet;
typedef struct dictType hashSetType; 
#define hashSetIterator dictIterator
#define hashSetGetIterator dictGetIterator
#define hashSetNext dictNext
#define hashSetReleaseIterator dictReleaseIterator
#define hashSetNode dictEntry 
#define hashSetNodeGetKey dictGetEntryKey 
extern hashSetType sdsHashSetDictType;

hashSet* hashSetCreate(hashSetType* dt);
void hashSetRelease(hashSet* hashSet);

void hashSetInit(hashSet* hashSet, dictType* dt);
void hashSetDestroy(hashSet* hashSet);


int hashSetAdd(hashSet* hashSet, void* element);
int hashSetRemove(hashSet* hashSet, void* element);
int hashSetContains(hashSet* hashSet, void* element);

size_t hashSetSize(hashSet* hashSet);


//set api
set* setCreateHash(hashSetType* type);
#endif