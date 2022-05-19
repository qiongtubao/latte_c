#include "list.h"

struct dlinkedListNode {
    struct dlinkedListNode *prev;
    struct dlinkedListNode *next;
    void *value;
} dlinkedListNode;

struct dlinkedListIter {
    listIter iter;
    struct dlinkedListNode *next;
    int direction;
} dlinkedListIter;

struct dlinkedList {
    list list;
    struct dlinkedListNode *head;
    struct dlinkedListNode *tail;
    unsigned long len;
} dlinkedList;


