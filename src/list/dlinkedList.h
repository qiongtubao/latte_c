#include "list.h"

struct dlinkedListNode {
    struct dlinkedListNode *prev;
    struct dlinkedListNode *next;
    void *value;
} dlinkedListNode;

struct dlinkedList {
    list list;
    struct dlinkedListNode *head;
    struct dlinkedListNode *tail;
    unsigned long len;
} dlinkedList;


