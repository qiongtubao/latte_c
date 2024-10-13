

#include "../utils/atomic.h"
#include "../sds/sds.h"

typedef int (comparator)(sds_t a_ele, void* a, sds_t b_ele, void* b);
typedef void (freeValue)(void* value);

typedef struct skiplist_node_t {
    sds_t ele;
    void* score;
    struct skiplist_node_t *backward;
    struct skiplistLevel {
        struct skiplist_node_t *forward;
        unsigned long span;
    } level[];
} skiplist_node_t;

typedef struct skiplist_t {
    struct skiplist_node_t *header, *tail;
    unsigned long length;
    int level;
    comparator* comparator;
} skiplist_t;

typedef struct skiplistIter {
    struct skiplist_t* list;
    struct skiplist_node_t* node;
} skiplistIter;


typedef struct {
    void *min, *max;
    int minex, maxex; /* are min or max exclusive? */
} zrangespec;

//skiplist method
struct skiplist_t* skipListNew(comparator comparator);
void skipListFree(skiplist_t* sl, freeValue* free_fn);
skiplist_node_t* skipListCreateNode(int level, void* value, sds_t ele);
void* skipListFreeNode(skiplist_node_t* node);

skiplist_node_t* skiplist_firstInRange(skiplist_t* sl, zrangespec* zs);
skiplist_node_t* skiplist_lastInRange(skiplist_t* sl, zrangespec* zs);

// int skipListContains(skiplist_t* skiplist, void* key);
skiplist_node_t* skipListInsert(skiplist_t* skiplist, void* value, sds_t ele);
void* skipListDelete(skiplist_t* skiplist, void* val, sds_t ele, skiplist_node_t** node);
// skiplist_node_t* skiplistFindGreaterOrEqual(const Key& key, Node** prev) const;
// int skiplistEqual(skiplist_t* skiplist, void* a, void* b);
// skiplist_node_t* skiplistFindLast(skiplist_t* skiplist);
// skiplist_node_t* skiplistFindLessThan(skiplist_t* skiplist, void* key);


//skiplistIter method
// int skiplistIterNext(skiplistIter* iter);
// int skiplistIterPrev(skiplistIter* iter);
// int skiplistIterVaild(skiplistIter* iter);
// void* skiplistIterKey(skiplistIter* iter);
// int skiplistSeek(skiplistIter* iter, void* key);
// int skiplistSeekToFirst(skiplistIter* iter);
// int skiplistSeekToLast(skiplistIter* iter);


//skiplistNode method
// int skiplistNodeSetNext(skiplist_node_t* node, int i, skiplist_node_t* n);
// skiplist_node_t* skiplistNodeNext(skiplist_node_t* node, int i);
// int skiplistNodeNoBarrierSetNext(skiplist_node_t* node, int i, skiplistrNode* n);
// skiplist_node_t* skiplistNodeNoBarrierNext(skiplist_node_t* node, int i);

//
int slValueGteMin(skiplist_t* sl, void* score, zrangespec* zs);
int slValueLteMax(skiplist_t* sl, void* score, zrangespec* zs);