

#include "../utils/atomic.h"

typedef int (comparator)(void* a, void* b);

typedef struct skiplistNode {
    void* key;
    struct skiplistNode *backward;
    struct skiplistLevel {
        struct skiplistNode *forward;
        unsigned long span;
    } level[];
} skiplistNode;

typedef struct skiplist {
    struct skiplistNode *header, *tail;
    unsigned long length;
    latteAtomic int level;
    int (comparator)(void* a, void* b);
} skiplist;

typedef struct skiplistIter {
    struct skiplist* list;
    struct skiplistNode* node;
} skiplistIter;


//skiplist method
struct skiplist* createSkipList(comparator comparator);
int skipListContains(skiplist* skiplist, void* key);
void insert(skiplist* skiplist, void* key);
skiplistNode* skiplistFindGreaterOrEqual(const Key& key, Node** prev) const;
int skiplistEqual(skiplist* skiplist, void* a, void* b);
skiplistNode* skiplistFindLast(skiplist* skiplist);
skiplistNode* skiplistFindLessThan(skiplist* skiplist, void* key);


//skiplistIter method
int skiplistIterNext(skiplistIter* iter);
int skiplistIterPrev(skiplistIter* iter);
int skiplistIterVaild(skiplistIter* iter);
void* skiplistIterKey(skiplistIter* iter);
int skiplistSeek(skiplistIter* iter, void* key);
int skiplistSeekToFirst(skiplistIter* iter);
int skiplistSeekToLast(skiplistIter* iter);


//skiplistNode method
int skiplistNodeSetNext(skiplistNode* node, int i, skiplistNode* n);
skiplistNode* skiplistNodeNext(skiplistNode* node, int i);
int skiplistNodeNoBarrierSetNext(skiplistNode* node, int i, skiplistrNode* n);
skiplistNode* skiplistNodeNoBarrierNext(skiplistNode* node, int i);

