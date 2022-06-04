

struct _listIter {
    void* (*next)(struct _listIter* iter);
};

typedef struct _listIter listIter;
struct _list
{
    int (*size)(struct _list* list);
    listIter* (*listIter)(struct _list* list, int direction);
    struct _list* (*insert)(struct _list* list, int index, void* value);
    void* (*remove)(struct _list* list, void* node);
    void (*release)(struct _list* list);
    void (*releaseIter)(void* iter);
    void* (*getNodeValue)(void* node);
};

typedef struct _list list;

/* Directions for iterators */
#define AL_START_HEAD 0
#define AL_START_TAIL 1


//Prototypes
typedef void* (*match_func)(void* src, void* match);

void listEmpty(list *list);
list *listAddNodeHead(list *list, void* value);
list *listAddNodeTail(list* list, void* value);
/**
 * @brief 
 * 
 * @param list 
 * @param index 
 * @param value 
 * @return list*
 * @example 
 *          1,2,3,4,5,
 *          insert 0 0
 *              0,1,2,3,4,5
 *          insert -1 0
 *              1,2,3,4,5,0
 *          insert 2 0
 *              1,2,0,3,4,5
 */
list *listInsertNode(list* list, int index, void* value);
long listSize(list* list);
void* listIndex(list* list, long index);
void* listGetNodeValue(list* list, void* node);

//double linked list
list* createDlinkedList();