

#include "skiplist.h"
#include <stdlib.h>
#include <stdio.h>

#define ZSKIPLIST_MAXLEVEL 32

#define ZSKIPLIST_P 0.25      /* Skiplist P = 1/4 */

skiplist_node_t* skipListCreateNode(int level, void* score, sds_t ele) {
    skiplist_node_t* node = zmalloc(sizeof(skiplist_node_t) + sizeof(skiplist_node_t) * level);
    node->backward = NULL;
    node->ele = ele;
    node->score = score;
    for (int i = 0; i < level; i++) {
        node->level[i].forward = NULL;
        node->level[i].span = 0;
    }
    return node;
}

void* skipListFreeNode(skiplist_node_t* node) {
    void* score = node->score;
    sds_delete(node->ele);
    zfree(node);
    return score;
}

void skipListFree(skiplist_t* sl, freeValue* free_value) {
    skiplist_node_t *node = sl->header->level[0].forward, *next;
    zfree(sl->header);
    while(node) {
        next = node->level[0].forward;
        void* v = skipListFreeNode(node);
        free_value(v);
        node = next;
    }
    zfree(sl);
}
/* Returns a random level for the new skiplist node we are going to create.
 * The return value of this function is between 1 and ZSKIPLIST_MAXLEVEL
 * (both inclusive), with a powerlaw-alike distribution where higher
 * levels are less likely to be returned. */
int skipListRandomLevel(void) {
    int level = 1;
    while ((random()&0xFFFF) < (ZSKIPLIST_P * 0xFFFF))
        level += 1;
    return (level<ZSKIPLIST_MAXLEVEL) ? level : ZSKIPLIST_MAXLEVEL;
}

skiplist_t* skipListNew(comparator* c) {
    int j;
    skiplist_t* sl;
    sl = (skiplist_t*)zmalloc(sizeof(skiplist_t));
    sl->level = 1;
    sl->length = 0;
    sl->header = skipListCreateNode(ZSKIPLIST_MAXLEVEL, 0, NULL);
    for (j = 0; j < ZSKIPLIST_MAXLEVEL; j++) {
        sl->header->level[j].forward = NULL;
        sl->header->level[j].span = 0;
    }
    sl->header->backward = NULL;
    sl->tail = NULL;
    sl->comparator = c;
    return sl;
}

skiplist_node_t* skipListInsert(skiplist_t *sl, void* score, sds_t ele) {
    skiplist_node_t *update[ZSKIPLIST_MAXLEVEL], *x;
    int rank[ZSKIPLIST_MAXLEVEL];
    int i, level;
    x = sl->header;

    for(i = sl->level-1; i >= 0; i--) {
        rank[i] = i == (sl->level - 1) ? 0: rank[i+1];
        while (x->level[i].forward &&
            sl->comparator(x->level[i].forward->ele, 
                x->level[i].forward->score, 
                ele, 
                score) < 0) {
            rank[i] += x->level[i].span;
            x = x->level[i].forward;
        } 
        update[i] = x;
    }
    level = skipListRandomLevel();
    if (level > sl->level) {
        for (i = sl->level; i < level; i++) {
            rank[i] = 0;
            update[i] = sl->header;
            update[i]->level[i].span = sl->length;
        }
        sl->level = level;
    }
    x = skipListCreateNode(level, score, ele);
    for (i = 0; i < level; i++) {
        x->level[i].forward = update[i]->level[i].forward;
        update[i]->level[i].forward = x;

        x->level[i].span = update[i]->level[i].span - (rank[0] - rank[i]);
        update[i]->level[i].span = (rank[0] - rank[i]) + 1;
    }

    for (i = level; i < sl->level; i++) {
        update[i]->level[i].span++;
    }
    x->backward = (update[0] == sl->header)? NULL: update[0];
    if (x->level[0].forward) {
        x->level[0].forward->backward = x;
    } else {
        sl->tail = x;
    }
    sl->length++;
    return x;
}

void skiplistDeleteNode(skiplist_t *sl, skiplist_node_t *x, skiplist_node_t **update) {
    int i;
    for (i = 0; i < sl->level; i++) {
        if (update[i]->level[i].forward == x) {
            update[i]->level[i].span += x->level[i].span - 1;
            update[i]->level->forward = x->level[i].forward;
        } else {
            update[i]->level[i].span -= 1;
        }
    }

    if (x->level[0].forward) {
        x->level[0].forward->backward = x->backward;
    } else {
        sl->tail = x->backward;
    }
    while(sl->level > 1 && sl->header->level[sl->level-1].forward == NULL) {
        sl->level--;
    }
    sl->length--;
}

void* skipListDelete(skiplist_t *sl, void* score, sds_t ele, skiplist_node_t **node) {
    skiplist_node_t *update[ZSKIPLIST_MAXLEVEL], *x;
    int i;

    x = sl->header;
    for(i = sl->level-1; i >= 0; i--) {
        while(x->level[i].forward &&
            sl->comparator(x->level[i].forward->ele, 
                x->level[i].forward->score, 
                ele, score) < 0) {
            x = x->level[i].forward;
        }
        update[i] = x;
    }

    x = x->level[0].forward;
    if (x && sl->comparator(NULL, x->score, NULL, score) == 0 
        && sds_cmp(x->ele, ele) == 0) {
        skiplistDeleteNode(sl, x, update);
        if (!node) {
            return skipListFreeNode(x);
        } else {
            *node = x;
            return x->score;
        }
    }
    return NULL;
}


int slValueGteMin(skiplist_t* sl, void* score, zrangespec* zs) {
    int result = sl->comparator(NULL, score, NULL, zs->min);
    return zs->minex ? result > 0: result >= 0;
}

int slValueLteMax(skiplist_t* sl, void* score, zrangespec* zs) {
    int result = sl->comparator(NULL, score, NULL, zs->max);
    return zs->maxex ? result < 0: result <= 0;
}


int slIsInRange(skiplist_t* sl, zrangespec *range) {
    skiplist_node_t *x;
    int result = sl->comparator(NULL, range->min, NULL, range->max);

    if (result > 0 || 
            (result == 0 && (range->minex || range->maxex))) {
        return 0;
    }
    x = sl->tail;
    if (x == NULL || !slValueGteMin(sl, x->score, range)) 
        return 0;
    x = sl->header->level[0].forward;
    if ( x == NULL || !slValueLteMax(sl, x->score, range))
        return 0;
    return 1;
}

skiplist_node_t* skiplist_firstInRange(skiplist_t* sl, zrangespec* range) {
    skiplist_node_t* x;
    int i;

    if (!slIsInRange(sl, range)) {
        return NULL;
    }
    x = sl->header;
    for( i = sl->level-1; i >= 0; i-- ) {
        while(x->level[i].forward &&
            !slValueGteMin(sl, x->level[i].forward->score, range)) {
            x = x->level[i].forward;
        }
    }
    
    x = x->level[0].forward;
    // assert(x != NULL);
    if (!slValueLteMax(sl, x->score, range)) return NULL;

    return x;
}

skiplist_node_t* skiplist_lastInRange(skiplist_t *sl, zrangespec* range) {
    skiplist_node_t *x;
    int i;

    if (!slIsInRange(sl, range)) return NULL;

    x = sl->header;
    for (i = sl->level-1; i >= 0; i--) {
        while (x->level[i].forward &&
            slValueLteMax(sl, x->level[i].forward->score, range))
                x = x->level[i].forward;
    }
    //assert(x != NULL);

    if (!slValueGteMin(sl, x->score, range)) {
        return NULL;
    }
    return x;
}

void slDeleteNode(skiplist_t *zsl, skiplist_node_t *x, skiplist_node_t **update) {
    int i;
    for (i = 0; i < zsl->level; i++) {
        if (update[i]->level[i].forward == x) {
            update[i]->level[i].span += x->level[i].span - 1;
            update[i]->level[i].forward = x->level[i].forward;
        } else {
            update[i]->level[i].span -= 1;
        }
    }
    if (x->level[0].forward) {
        x->level[0].forward->backward = x->backward;
    } else {
        zsl->tail = x->backward;
    }
    while(zsl->level > 1 && zsl->header->level[zsl->level-1].forward == NULL)
        zsl->level--;
    zsl->length--;
}

/* Free the specified skiplist node. The referenced SDS string representation
 * of the element is freed too, unless node->ele is set to NULL before calling
 * this function. */
void zslFreeNode(skiplist_t* sl, skiplist_node_t *node) {
    sds_delete(node->ele);
    zfree(node);
}

