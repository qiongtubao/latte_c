#include "list.h"
#include <stdlib.h>

void listEmpty(list *list) {
    listIter* iter = list->listIter(list, AL_START_HEAD);
    void* node;
    while ((node = iter->next(iter)) != NULL) {
        list->remove(list, node);
    }
    list->releaseIter(iter);
}

void listRelease(list* list) {
    listEmpty(list);   
    list->release(list);
}

list *listAddNodeHead(list *list, void* value) {
    return list->insert(list, 0, value);
}

list *listAddNodeTail(list* list, void* value) {
    return list->insert(list, -1, value);
}

list *listInsertNode(list* list, int index, void* value) {
    return list->insert(list, index, value);
}

void listDelNode(list *list, void* node) {
    list->remove(list, node);
}

listIter *listGetIterator(list* list, int direction) {
    return list->listIter(list, direction);
}

void listReleaseIterator(list* list, listIter* iter) {
    list->releaseIter(iter);
}

void* listNext(listIter *iter) {
    return iter->next(iter);
}



void* listSearchKey(list *list, void *key, match_func match) { 
    listIter* iter;
    void* node;
    iter = list->listIter(list, AL_START_HEAD);
    while((node = iter->next(iter)) != NULL) {
        if (match != NULL) {
            if (match(list->getNodeValue(node), key)) {
                return node;
            }
        } else {
            if (key == list->getNodeValue(node)) {
                return node;
            }
        }
    }
    list->releaseIter(iter);
    return NULL;
}


void* listIndex(list *list, long index) {
    listIter* iter;
    void* node;
    long count = index;
    if (index >= 0) {
        iter = list->listIter(list, AL_START_HEAD);
    } else {
        iter = list->listIter(list, AL_START_TAIL);
        count = -index;
    }
    while((node = iter->next(iter)) != NULL && count != 0) {
        count--;
    }
    return node;
}


long listSize(list* list) {
    return list->size(list);
}

void* listGetNodeValue(list* list, void* node) {
    return list->getNodeValue(node);
}