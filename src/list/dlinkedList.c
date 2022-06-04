#include "dlinkedList.h"
#include <stdlib.h>
#include <zmalloc.h>

int dlinkedListSize(list* list) {
    struct dlinkedList* dl = (struct dlinkedList *)list;
    return dl->len;
}



list* dlinkedInsertNode(struct dlinkedList* dl, struct dlinkedListNode *old_node, void* value, int after) {
    struct dlinkedListNode *node;
    if ((node = zmalloc(sizeof(*node))) == NULL) {
        return NULL;
    }
    node->value = value;
    if (after) {
        node->prev = old_node;
        node->next = old_node->next;
        if (dl->tail == old_node) {
            dl->tail = node;
        }
    } else {
        node->next = old_node;
        node->prev = old_node->prev;
        if (dl->head == old_node) {
            dl->head = node;
        }
    }
    if (node->prev != NULL) {
        node->prev->next = node;
    }
    if (node->next != NULL) {
        node->next->prev = node;
    }
    dl->len++;
    return (list*)dl;
}



list* dlinkedListAddNodeHead(struct dlinkedList* dl, void* value) {
    struct dlinkedListNode *node;
    if ((node = zmalloc(sizeof(*node))) == NULL) 
        return NULL;
    node->value = value;
    if (dl->len == 0) {
        dl->head = dl->tail = node;
        node->prev = node->next = NULL;
    } else {
        node->prev = NULL;
        node->next = dl->head;
        dl->head->prev = node;
        dl->head = node;
    }
    dl->len++;
    return (list*)dl;
}

list* dlinkedListAddNodeTail(struct dlinkedList* dl, void* value) {
    struct dlinkedListNode *node;
    if ((node = zmalloc(sizeof(*node))) == NULL) 
        return NULL;
    node->value = value;
    if (dl->len == 0) {
        dl->head = dl->tail = node;
        node->prev = node->next = NULL;
    } else {
        node->prev = dl->tail;
        node->next = NULL;
        dl->tail->next = node;
        dl->tail = node;
    }
    dl->len++;
    return (list*)dl;
}

// dlinkedListIter (*listIter)(list* list);
list* dlinkedListInsert(list* list, int index, void* value) {
    struct dlinkedList* dl = (struct dlinkedList *)list;
    if (index == 0) {
        return dlinkedListAddNodeHead(dl, value);
    } else if (index == -1) {
        return dlinkedListAddNodeTail(dl, value);
    } else {
        struct dlinkedListNode* node;
        if(index > 0)  {
            node = (struct dlinkedListNode*)listIndex(list, index - 1);    
        } else {
            node = (struct dlinkedListNode*)listIndex(list,  listSize(list) + index);
        }
        if (node == NULL) return NULL;
        return dlinkedInsertNode(dl, node, value, 1);
    } 
}


void* dlinkedRemove(list* list, void* node) {
    struct dlinkedList* dl = (struct dlinkedList *)list;
    struct dlinkedListNode* dlnode = (struct dlinkedListNode*)node;
    if (dlnode->prev) {
        dlnode->prev->next = dlnode->next;
    } else {
        dl->head = dlnode->next;
    }
    if (dlnode->next) {
        dl->tail = dlnode->prev;
    } else {
        dl->tail = dlnode->prev;
    }
    void* value = dlnode->value;
    zfree(dlnode);
    dl->len--;
    return dlnode->value;
}

void* dlinkedListNext(listIter *iter) {
    struct dlinkedListIter* dliter = (struct dlinkedListIter*) iter;
    struct dlinkedListNode* current = dliter->next;
    if (current != NULL) {
        if (dliter->direction == AL_START_HEAD) {
            dliter->next = current->next;
        } else {
            dliter->next = current->prev;
        }
    }
    return current;
}

listIter* dlinkedListGetIter(list* list, int direction) {
    struct dlinkedListIter* iter;
    struct dlinkedList* dl = (struct dlinkedList*)list;
    if ((iter = zmalloc(sizeof(*iter))) == NULL) return NULL;
    if (direction == AL_START_HEAD) {
        iter->next = dl->head;
    } else {
        iter->next = dl->tail;
    }
    iter->direction = direction;
    iter->iter.next = dlinkedListNext;
    return (listIter*)iter;
}

void dlinkedReleaseIter(void* it) {
    zfree(it);
}

void* dlinkedListGetNodeValue(void* n) {
    struct dlinkedListNode* node = (struct dlinkedListNode*)n;
    return node->value;
}

// public 

list* createDlinkedList() {
    struct dlinkedList* dl = zmalloc(sizeof(struct dlinkedList));
    dl->list.insert = dlinkedListInsert;
    dl->list.remove = dlinkedRemove;
    dl->list.size = dlinkedListSize;
    dl->list.listIter = dlinkedListGetIter;
    dl->list.releaseIter = dlinkedReleaseIter;
    dl->list.getNodeValue = dlinkedListGetNodeValue;
    dl->head = NULL;
    dl->tail = NULL;
    dl->len = 0;
    return (list*)dl;
}
