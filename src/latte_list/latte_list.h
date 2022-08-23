
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>


typedef struct listNodeIterType {
    int (*set_value)(struct latte_list* list, struct latte_list_node* node, void* value);
    void* (*get_value)(struct latte_list* list, struct latte_list_node* node);
    struct latte_list_node* (*next)(struct latte_list* list, struct latte_list_node* node);
    struct latte_list_node* (*prev)(struct latte_list* list, struct latte_list_node* node);
} listNodeType;

typedef struct latte_list_node {
    listNodeType* type;
} latte_list_node; 

typedef struct latte_list_iter {
    struct latte_list_node* node;
    struct latte_list* list;
    int direction;
} latte_list_iter;

typedef struct listType {
    int (*listLength)(struct latte_list *list);
    struct latte_list* (*listAddNode)(struct latte_list* list, struct latte_list_node* node, const void* value, int after);
    void (*freeList)(struct latte_list* list);
    void (*dupList)(struct latte_list* list);
    void* (*index)(struct latte_list* list, int index);
    void* (*del)(struct latte_list* list, int index);
    int (*indexOf)(struct latte_list* list, struct latte_list_node* node);
    struct latte_list_iter* (*getIter)(struct latte_list* list, int direction);
    struct latte_list_node* (*createNode)(struct latte_list* list);
    int (*match)(struct latte_list* list, void* key);
} listType;

typedef struct latte_list {
    listType* type;
} latte_list;


/* list type */
struct latte_list* createList();
struct latte_list* createSkipList();
struct latte_list* createZiplist();
struct latte_list* createArrayList();

/* Prototypes */
static inline int latteListLength(struct latte_list* list) {
    return list->type->listLength(list);
}

// list *listCreate(void);
// void listRelease(list *list);
// void listEmpty(list *list);
// void listEmpty(latte_list* list);
latte_list *latteListAddNodeHead(latte_list *list, void *value);
latte_list *latteListAddNodeTail(latte_list *list, void *value);
latte_list *latteListInsertNode(latte_list *list, latte_list_node *old_node, void *value, int after);
void *latteListIndex(latte_list *list, int index);
// void* latteListSearchKey(latte_list* list, void* key);
void* latteListDelByIndex(latte_list* list, int index);
// void listDelNode(latte_list *list, latte_list_Node *node);

// listIter *listGetIterator(list *list, int direction);
// listNode *listNext(listIter *iter);
// void listReleaseIterator(listIter *iter);
// list *listDup(list *orig);
// listNode *listSearchKey(list *list, void *key);
// listNode *listIndex(list *list, long index);
// void listRewind(list *list, listIter *li);
// void listRewindTail(list *list, listIter *li);
// void listRotateTailToHead(list *list);
// void listRotateHeadToTail(list *list);
// void listJoin(list *l, list *o);


