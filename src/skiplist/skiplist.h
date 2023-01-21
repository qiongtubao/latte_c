

#include "../utils/atomic.h"
#include "../sds/sds.h"

typedef int (comparator)(sds a_ele, void* a, sds b_ele, void* b);
typedef void (freeValue)(void* value);

typedef struct skiplistNode {
    sds ele;
    void* score;
    struct skiplistNode *backward;
    struct skiplistLevel {
        struct skiplistNode *forward;
        unsigned long span;
    } level[];
} skiplistNode;

typedef struct skiplist {
    struct skiplistNode *header, *tail;
    unsigned long length;
    int level;
    comparator* comparator;
} skiplist;

typedef struct skiplistIter {
    struct skiplist* list;
    struct skiplistNode* node;
} skiplistIter;


typedef struct {
    void *min, *max;
    int minex, maxex; /* are min or max exclusive? */
} zrangespec;

//skiplist method
struct skiplist* skipListNew(comparator comparator);
void skipListFree(skiplist* sl, freeValue* free_fn);
skiplistNode* skipListCreateNode(int level, void* value, sds ele);
void* skipListFreeNode(skiplistNode* node);

skiplistNode* skiplistFirstInRange(skiplist* sl, zrangespec* zs);
skiplistNode* skiplistLastInRange(skiplist* sl, zrangespec* zs);

// int skipListContains(skiplist* skiplist, void* key);
skiplistNode* skipListInsert(skiplist* skiplist, void* value, sds ele);
void* skipListDelete(skiplist* skiplist, void* val, sds ele, skiplistNode** node);
// skiplistNode* skiplistFindGreaterOrEqual(const Key& key, Node** prev) const;
// int skiplistEqual(skiplist* skiplist, void* a, void* b);
// skiplistNode* skiplistFindLast(skiplist* skiplist);
// skiplistNode* skiplistFindLessThan(skiplist* skiplist, void* key);


//skiplistIter method
// int skiplistIterNext(skiplistIter* iter);
// int skiplistIterPrev(skiplistIter* iter);
// int skiplistIterVaild(skiplistIter* iter);
// void* skiplistIterKey(skiplistIter* iter);
// int skiplistSeek(skiplistIter* iter, void* key);
// int skiplistSeekToFirst(skiplistIter* iter);
// int skiplistSeekToLast(skiplistIter* iter);


//skiplistNode method
// int skiplistNodeSetNext(skiplistNode* node, int i, skiplistNode* n);
// skiplistNode* skiplistNodeNext(skiplistNode* node, int i);
// int skiplistNodeNoBarrierSetNext(skiplistNode* node, int i, skiplistrNode* n);
// skiplistNode* skiplistNodeNoBarrierNext(skiplistNode* node, int i);

//
int slValueGteMin(skiplist* sl, void* score, zrangespec* zs);
int slValueLteMax(skiplist* sl, void* score, zrangespec* zs);