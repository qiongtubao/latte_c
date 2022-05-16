

typedef struct listIter {
    void* (*next)(listIter* iter);
} listIter;

struct _list
{
    int (*size)(list* list);
    listIter (*listIter)(list* list);
    list* (*insert)(list* list, void* node, int after);
    void* (*remove)(list* list, void* node);
    void (*releaseIter)(list* list, void* iter);
} _list;

typedef struct _list list;


//void** (*listToArray)(list* list);
//int (*listContains)(list* list);
//int listTypeConversion(list* src, list* target);
// int (*isEmpty)(list* list); 
// indexOf()



//double linked list
list* createDlinkedList();