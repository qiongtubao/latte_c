


#ifndef __LATTE_SET_H
#define __LATTE_SET_H

#include <dict/dict.h>
#include <dict_plugins/dict_plugins.h>
#include <stdbool.h>
typedef dict set;

#define setIterator dictIterator
#define setGetIterator dictGetIterator
#define setNext dictNext
#define setReleaseIterator dictReleaseIterator
#define setNode dictEntry 
#define setNext dictNext 
#define setNodeGetKey dictGetEntryKey 
extern dictType sdsSetDictType;

set* setCreate(dictType* dt);
void setRelease(set* set);

void setInit(set* set, dictType* dt);
void setDestroy(set* set);


int setAdd(set* set, void* element);
int setRemove(set* set, void* element);
int setContains(set* set, void* element);

size_t setSize(set* set);
#endif