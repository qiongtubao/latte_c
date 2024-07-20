
#ifndef __LATTE_HASH_SET_H
#define __LATTE_HASH_SET_H

#include <dict/dict.h>
#include <dict_plugins/dict_plugins.h>
typedef dict hashSet;

#define hashSetIterator dictIterator
#define hashSetGetIterator dictGetIterator
#define hashSetNext dictNext
#define hashSetReleaseIterator dictReleaseIterator
#define hashSetNode dictEntry 
#define hashSetNext dictNext 
#define hashSetNodeGetKey dictGetEntryKey 
extern dictType sdshashSetDictType;

hashSet* hashSetCreate(dictType* dt);
void hashSetRelease(hashSet* hashSet);

void hashSetInit(hashSet* hashSet, dictType* dt);
void hashSetDestroy(hashSet* hashSet);


int hashSetAdd(hashSet* hashSet, void* element);
int hashSetRemove(hashSet* hashSet, void* element);
int hashSetContains(hashSet* hashSet, void* element);

size_t hashSetSize(hashSet* hashSet);

#endif