#include "dlinkedList.h"
#include <stdlib.h>

int dlinkedListSize(list* list) {
    struct dlinkedList* dl = (struct dlinkedList *)list;
    return dl->len;
}



list* dlinkedInsertNode(struct dlinkedList* list, struct dlinkedListNode *old_node, void* value, int after) {
    struct dlinkedListNode *node;
    if ((node = zmalloc(sizeof(*node))) == NULL) {
        return NULL;
    }
    node->value = value;
    if (after) {
        node->prev = old_node;
        node->next = old_node->next;
        if (list->tail == old_node) {
            list->tail = node;
        }
    } else {
        node->next = old_node;
        node->prev = old_node->prev;
        if (list->head == old_node) {
            list->head = node;
        }
    }
    if (node->prev != NULL) {
        node->prev->next = node;
    }
    if (node->next != NULL) {
        node->next->prev = node;
    }
    list->len++;
    return list;
}

list* dlinkedListAddNodeTail(struct dlinkedList* dl, void* value) {

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
    return dl;
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
    return dl;
}

// dlinkedListIter (*listIter)(list* list);
list* dlinkedListInsert(list* list, void* node, void* value, int after) {
    struct dlinkedList* dl = (struct dlinkedList *)list;
    if (node == NULL) {
        if (after) {
            return dlinkedListAddNodeTail(dl, value);
        } else {
            return dlinkedListAddNodeHead(dl, value);
        }
    } else {
        return dlinkedInsertNode(dl, node, value, after);
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


listIter* dlinkedListIter(list* list, int direction) {
    
}

void dlinkedReleaseIter(list* list, listIter* iter) {
    struct dlinkedList* dl = zmalloc(sizeof(struct dlinkedList));
}

// public 

list* createDlinkedList() {
    struct dlinkedList* dl = zmalloc(sizeof(struct dlinkedList));
    dl->list.insert = dlinkedInsertNode;
    dl->list.remove = dlinkedRemove;
    dl->list.size = dlinkedListSize;
    dl->list.listIter = dlinkedListIter;
    dl->list.releaseIter = dlinkedReleaseIter;
    dl->head = NULL;
    dl->tail = NULL;
    dl->len = 0;
    return dl;
}
