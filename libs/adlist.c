#include <stdlib.h>
#include "adlist.h"
#include "zmalloc.h"

list *listCreate(void) 
{
    struct list *list;
    if ((list = zmalloc(sizeof(*list))) == NULL) return NULL;
    list->head = list->tail = NULL;
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;
    return list;
}

void listEmpty(list *list) 
{
    unsigned long len;
    listNode *current, *next;
    current = list->head;
    len = list->len;
    while(len--) {
        next = current->next;
        if (list->free) list->free(current->value);
        zfree(current);
        current = next;
    }
    list->head = list->tail = NULL;
    list->len = 0;
}

void listRelease(list *list) 
{
    listEmpty(list);
    zfree(list);
}

list *listAddNodeHead(list *list, void *value) 
{
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL) return NULL;
    node->value = value;
    if (list->len == 0) {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
    list->len++;
    return list;
}

list *listAddNodeTail(list *list, void *value) {
    listNode *node;
    if ((node = zmalloc(sizeof(*node))) == NULL) return NULL;

    node->value = value;
    if (list->len == 0) {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }
    list->len++;
    return list;
}

list *listInsertNode(list *list ,listNode *old_node, void *value, int after) {
    listNode *node;
    if ((node = zmalloc(sizeof(*node))) == NULL) return NULL;
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
    if(node->next != NULL) {
        node->next->prev = node;
    }
    list->len++;
    return list;
}

void listDelNode(list *list, listNode *node) 
{
    if (node->prev) 
        node->prev->next = node->next;
    else 
        list->head = node->next;
    if (node->next)
        node->next->prev = node->prev;
    else 
        list->tail =  node->prev;
    if (list->free) list->free(node->value);
    zfree(node);
    list->len--;
}

listIter *listGetIterator(list *list, int direction)
{
    listIter *iter;
    if ((iter = zmalloc(sizeof(*iter))) == NULL) return NULL;
    if (direction == AL_START_HEAD)
        iter->next = list->head;
    else 
        iter->next = list->tail;
    iter->direction = direction;
    return iter;
}

void listReleaseIterator(listIter *iter) {
    zfree(iter);
}

void listRewind(list *list, listIter *li) {
    li->next = list->head;
    li->direction = AL_START_HEAD;
}

void listRewindTail(list *list, listIter *li) {
    li->next = list->tail;
    li->direction = AL_START_TAIL;
}

listNode *listNext(listIter *iter) 
{
    listNode *current = iter->next;
    if (current != NULL) {
        if(iter->direction == AL_START_HEAD)
            iter->next = current->next;
        else 
            iter->next = current->prev;   
    }
    return current;
}

list *listDup(list *orig) 
{
    list *copy;
    listIter iter;
    listNode *node;

    if ((copy = listCreate()) == NULL)
        return NULL;
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;
    listRewind(orig, &iter);
    while((node = listNext(&iter)) != NULL) {
        void *value;

        if (copy->dup) {
            value = copy->dup(node->value);
            if (value == NULL) {
                listRelease(copy);
                return NULL;
            }
        } else
            value = node->value;
        if (listAddNodeTail(copy, value) == NULL) {
            listRelease(copy);
            return NULL;
        }
    }
    return copy;
}

listNode *listSearchKey(list *list, void *key)
{
    listIter iter;
    listNode *node;

    listRewind(list, &iter);
    while((node = listNext(&iter)) != NULL) {
        if (list->match) {
            if (list->match(node->value, key)) {
                return node;
            }
        } else {
            if (key == node->value) {
                return node;
            }
        }
    }
    return NULL;
}

listNode *listIndex(list *list, long index) {
    listNode *n;

    if (index < 0) {
        index = (-index)-1;
        n = list->tail;
        while(index-- && n) n = n->prev;
    } else {
        n = list->head;
        while(index-- && n) n = n->next;
    }
    return n;
}

void listRotate(list *list) {
    listNode *tail = list->tail;

    if (listLength(list) <= 1) return;

    /* Detach current tail */
    list->tail = tail->prev;
    list->tail->next = NULL;
    /* Move it as head */
    list->head->prev = tail;
    tail->prev = NULL;
    tail->next = list->head;
    list->head = tail;
}

void listJoin(list *l, list *o) {
    if (o->head)
        o->head->prev = l->tail;

    if (l->tail)
        l->tail->next = o->head;
    else
        l->head = o->head;

    if (o->tail) l->tail = o->tail;
    l->len += o->len;

    /* Setup other as an empty list. */
    o->head = o->tail = NULL;
    o->len = 0;
}
#if defined(LIST_TEST_MAIN)
#include <stdio.h>
#include "testhelp.h"
#include "limits.h"
#include <string.h>
#define UNUSED(x) (void)(x)
int listTest(void) {
    {
        list* x = listCreate();
        int a = 1, b = 2 , c = 3;
        listNode* node = NULL, *before = NULL, *after = NULL;
        listAddNodeHead(x, &a);
        test_cond("add list node head",
                listLength(x) == 1 && listIndex(x, 0) != NULL)
        
        node = listIndex(x, 0);
        test_cond("check listnode head value",
            listNodeValue(node) == &a)
        listEmpty(x);
        test_cond("empty list",
                listLength(x) == 0 && listIndex(x, 0) == NULL)
        
        listAddNodeTail(x, &b);
        test_cond("add list node tail",
                listLength(x) == 1 && listIndex(x, -1) != NULL)
        node = listIndex(x, -1);
        test_cond("check listnode tail value",
            listNodeValue(node) == &b)

        listInsertNode(x, node, &a, 0);
        before = listIndex(x, 0);
        test_cond("insert before",
                listLength(x) == 2 && before != NULL && listNodeValue(before) == &a && node == before->next && before == node->prev)
        
        listInsertNode(x, node, &c, 1);
        after = listIndex(x, 2);
        test_cond("insert after",
                listLength(x) == 3 && after != NULL && listNodeValue(after) == &c && after == node->next && node == after->prev)
        
        listDelNode(x, node);
        test_cond("del node", 
                listLength(x) == 2 && listIndex(x, 1) != node)

        {
            listIter* iter = listGetIterator(x, 0);
            int num = 0;
            while((node = listNext(iter)) != NULL) {
                printf("[%d]\n", *(int*)(node->value));
                num ++;
            }
            listReleaseIterator(iter);
            test_cond("for each iter num", num == 2)

            list* y = listDup(x);
            listRewind(y, iter);
            num = 0;
            while((node = listNext(iter)) != NULL) {
                test_cond("check dup iter", (node->value) == listNodeValue(listIndex(x, num)))
                num ++;
            }
            listReleaseIterator(iter);

            listRotate(y);
            listRewindTail(y, iter);
            num = 0;
            while((node = listNext(iter)) != NULL) {
                test_cond("check rotate iter", (node->value) == listNodeValue(listIndex(x, num)))
                num ++;
            }
            listNode* find = listSearchKey(x, &a);
            test_cond("check find", listNodeValue(find) == &a);

            listEmpty(y);
            listAddNodeHead(y, &b);
            listJoin(x, y);
            test_cond("check join", listLength(x) == 3 && listLength(y) == 0 && listNodeValue(listIndex(x, 2)) == &b)
        }
        listRelease(x);
    }
    test_report();
    return 0;
}


#endif

#ifdef LIST_TEST_MAIN
int main(void) {
    return listTest();
}
#endif
