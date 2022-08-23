#include "latte_list.h"
#include "zmalloc/zmalloc.h"




typedef struct listNodeValue {
    struct listNodeValue* prev;
    struct listNodeValue* next;
    void* value;
} listNodeValue;


typedef struct listNode {
    struct latte_list_node type; 
    struct listNodeValue* value;
} listNode;

typedef struct list {
    struct latte_list parent;
    listNodeValue* head;
    listNodeValue* tail;
    unsigned long len;
} list;

int list_length(struct latte_list *ll) {
    list* l = (list*)ll;
    return l->len;
}


struct latte_list* listAddNode(struct latte_list* list_, struct latte_list_node* old_node_, void* value, int after) {
    struct list *list = list_;
    listNodeValue* old_node = old_node != NULL? ((listNodeValue*)(old_node_))->value: NULL;
    listNodeValue *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = value;
    if (old_node == NULL) {
        if (after == 0) {
            //add head
            if (list->len == 0) {
                list->head = list->tail = node;
                node->prev = node->next = NULL;
            } else {
                node->prev = NULL;
                node->next = list->head;
                list->head->prev = node;
                list->head = node;
            }
        } else {
            //add tail
            if (list->len == 0) {
                list->head = list->tail = node;
                node->prev = node->next = NULL;
            } else {
                node->prev = list->tail;
                node->next = NULL;
                list->tail->next = node;
                list->tail = node;
            }
        }
        
    } else {

        //add node
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
    }
    list->len++;
    return list;
}

listNodeValue* listIndex_(list *list, long index) {
    listNodeValue *n;

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
/* Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 the penultimate
 * and so on. If the index is out of range NULL is returned. */
void* listIndex(list *list, long index) {
    listNodeValue* n = listIndex_(list, index);
    return n->value;
}

void freeListNodeValue(listNodeValue* value) {
    zfree(value);
}

void* listDel(latte_list* list, int index) {
    struct list* l = list;
    listNodeValue* n = listIndex_(list, index);
    if (n->next != NULL) {
        n->next->prev = n->prev;
    } else {
        l->tail = n->prev;
    }
    
    if (n->prev != NULL) {
        n->prev->next = n->next;
    } else {
        l->head = n->next;
    }

    void* v = n->value;
    freeListNodeValue(n);
    l->len--;
    return v;
}
typedef void (freeValueType)(void* v);
void* freeList(list* list, freeValueType* freeV) {
   
}

struct listType list_type = {
    .listLength = list_length,
    .listAddNode = listAddNode,
    .index = listIndex,
    .del = listDel,
    .freeList = freeList
};

struct latte_list* createList() {
    struct list *l;
    if ((l = zmalloc(sizeof(*l))) == NULL) return NULL;
    l->head = l->tail = NULL;
    l->len = 0;
    l->parent.type = &list_type;
    return (latte_list*)l;
}



