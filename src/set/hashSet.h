
#ifndef __LATTE_HASH_SET_H
#define __LATTE_HASH_SET_H

#include <dict/dict.h>
#include <dict/dict_plugins.h>
#include "set.h"

typedef struct dict_t hashSet;
typedef struct dict_func_t hashSetType; 
#define hashSetIterator dict_iterator_t
#define hashSetGetIterator dict_get_iterator
#define hashSetNext dict_next
#define hashSetReleaseIterator dict_iterator_delete
#define hashSetNode dict_entry_t
#define hashSetNodeGetKey dict_get_entry_key 
extern hashSetType sdsHashSetDictType;

hashSet* hashSetCreate(hashSetType* dt);
void hashSetRelease(hashSet* hashSet);

void hashSetInit(hashSet* hashSet, dict_func_t* dt);
void hashSetDestroy(hashSet* hashSet);


int hashSetAdd(hashSet* hashSet, void* element);
int hashSetRemove(hashSet* hashSet, void* element);
int hashSetContains(hashSet* hashSet, void* element);

size_t hashSetSize(hashSet* hashSet);


//set api
set* setCreateHash(hashSetType* type);
#endif