#include <stdio.h>
#include "quicklist.h"
#include "utils/utils.h"
#include <assert.h>
#include <limits.h>
/* packed_threshold is initialized to 1gb*/
static size_t packed_threshold = (1 << 30);

/* set threshold for PLAIN nodes, the real limit is 4gb */
#define isLargeElement(size) ((size) >= packed_threshold)

#define SIZE_ESTIMATE_OVERHEAD 8

/* Minimum listpack size in bytes for attempting compression. */
#define MIN_COMPRESS_BYTES 48
// int quicklistisSetPackedThreshold(size_t sz) {
//     if (sz > (1ull<<32) - (1 << 20)) {
//         return 0;
//     } else if (sz == 0) {
//         sz = (1 << 30);
//     }
//     packed_threshold = sz;
//     return 1;
// }

#define sizeMeetsSafetyLimit(sz) ((sz) <= SIZE_SAFETY_LIMIT)

/* Maximum size in bytes of any multi-element listpack.
 * Larger values will live in their own isolated listpacks.
 * This is used only if we're limited by record count. when we're limited by
 * size, the maximum limit is bigger, but still safe.
 * 8k is a recommended / default size limit */
#define SIZE_SAFETY_LIMIT 8192

/* Minimum size reduction in bytes to store compressed quicklistNode data.
 * This also prevents us from storing compression if the compression
 * resulted in a larger size than the original data. */
#define MIN_COMPRESS_IMPROVE 8

/* Optimization levels for size-based filling.
 * Note that the largest possible limit is 64k, so even if each record takes
 * just one byte, it still won't overflow the 16 bit count field. */
static const size_t optimization_level[] = {4096, 8192, 16384, 32768, 65536};

#define quicklistNodeUpdateSz(node)                                            \
    do {                                                                       \
        (node)->sz = lpBytes((node)->entry);                                   \
    } while (0)

static quicklist_node_t* __quicklist_newPlainNode(void *value, size_t sz) {
    quicklist_node_t *new_node = quicklist_newNode();
    new_node->entry = zmalloc(sz);
    new_node->container = QUICKLIST_NODE_CONTAINER_PLAIN;
    memcpy(new_node->entry, value, sz);
    new_node->sz = sz;
    new_node->count++;
    return new_node;
}

/* Compress the listpack in 'node' and update encoding details.
 * Returns 1 if listpack compressed successfully.
 * Returns 0 if compression failed or if listpack too small to compress. */
int __quicklistCompressNode(quicklist_node_t *node) {
#ifdef REDIS_TEST
    node->attempted_compress = 1;
#endif
    if (node->dont_compress) return 0;

    /* validate that the node is neither
     * tail nor head (it has prev and next)*/
    assert(node->prev && node->next);

    node->recompress = 0;
    /* Don't bother compressing small values */
    if (node->sz < MIN_COMPRESS_BYTES)
        return 0;

    quicklistLZF *lzf = zmalloc(sizeof(*lzf) + node->sz);

    /* Cancel if compression fails or doesn't compress small enough */
    if (((lzf->sz = lzf_compress(node->entry, node->sz, lzf->compressed,
                                 node->sz)) == 0) ||
        lzf->sz + MIN_COMPRESS_IMPROVE >= node->sz) {
        /* lzf_compress aborts/rejects compression if value not compressible. */
        zfree(lzf);
        return 0;
    }
    lzf = zrealloc(lzf, sizeof(*lzf) + lzf->sz);
    zfree(node->entry);
    node->entry = (unsigned char *)lzf;
    node->encoding = QUICKLIST_NODE_ENCODING_LZF;
    return 1;
}

/* Compress only uncompressed nodes. */
#define quicklistCompressNode(_node)                                           \
    do {                                                                       \
        if ((_node) && (_node)->encoding == QUICKLIST_NODE_ENCODING_RAW) {     \
            __quicklistCompressNode((_node));                                  \
        }                                                                      \
    } while (0)


#define quicklistAllowsCompression(_ql) ((_ql)->compress != 0)

/* Uncompress the listpack in 'node' and update encoding details.
 * Returns 1 on successful decode, 0 on failure to decode. */
int __quicklistDecompressNode(quicklist_node_t *node) {
#ifdef REDIS_TEST
    node->attempted_compress = 0;
#endif
    node->recompress = 0;

    void *decompressed = zmalloc(node->sz);
    quicklistLZF *lzf = (quicklistLZF *)node->entry;
    if (lzf_decompress(lzf->compressed, lzf->sz, decompressed, node->sz) == 0) {
        /* Someone requested decompress, but we can't decompress.  Not good. */
        zfree(decompressed);
        return 0;
    }
    zfree(lzf);
    node->entry = decompressed;
    node->encoding = QUICKLIST_NODE_ENCODING_RAW;
    return 1;
}
/* Decompress only compressed nodes. */
#define quicklistDecompressNode(_node)                                         \
    do {                                                                       \
        if ((_node) && (_node)->encoding == QUICKLIST_NODE_ENCODING_LZF) {     \
            __quicklistDecompressNode((_node));                                \
        }                                                                      \
    } while (0)
/* Force 'quicklist' to meet compression guidelines set by compress depth.
 * The only way to guarantee interior nodes get compressed is to iterate
 * to our "interior" compress depth then compress the next node we find.
 * If compress depth is larger than the entire list, we return immediately. */
void __quicklistCompress(const quicklist_t *quicklist,
                                      quicklist_node_t *node) {
    if (quicklist->len == 0) return;

    /* The head and tail should never be compressed (we should not attempt to recompress them) */
    assert(quicklist->head->recompress == 0 && quicklist->tail->recompress == 0);

    /* If length is less than our compress depth (from both sides),
     * we can't compress anything. */
    if (!quicklistAllowsCompression(quicklist) ||
        quicklist->len < (unsigned int)(quicklist->compress * 2))
        return;

#if 0
    /* Optimized cases for small depth counts */
    if (quicklist->compress == 1) {
        quicklist_node_t *h = quicklist->head, *t = quicklist->tail;
        quicklistDecompressNode(h);
        quicklistDecompressNode(t);
        if (h != node && t != node)
            quicklistCompressNode(node);
        return;
    } else if (quicklist->compress == 2) {
        quicklist_node_t *h = quicklist->head, *hn = h->next, *hnn = hn->next;
        quicklist_node_t *t = quicklist->tail, *tp = t->prev, *tpp = tp->prev;
        quicklistDecompressNode(h);
        quicklistDecompressNode(hn);
        quicklistDecompressNode(t);
        quicklistDecompressNode(tp);
        if (h != node && hn != node && t != node && tp != node) {
            quicklistCompressNode(node);
        }
        if (hnn != t) {
            quicklistCompressNode(hnn);
        }
        if (tpp != h) {
            quicklistCompressNode(tpp);
        }
        return;
    }
#endif

    /* Iterate until we reach compress depth for both sides of the list.a
     * Note: because we do length checks at the *top* of this function,
     *       we can skip explicit null checks below. Everything exists. */
    quicklist_node_t *forward = quicklist->head;
    quicklist_node_t *reverse = quicklist->tail;
    int depth = 0;
    int in_depth = 0;
    while (depth++ < quicklist->compress) {
        quicklistDecompressNode(forward);
        quicklistDecompressNode(reverse);

        if (forward == node || reverse == node)
            in_depth = 1;

        /* We passed into compress depth of opposite side of the quicklist
         * so there's no need to compress anything and we can exit. */
        if (forward == reverse || forward->next == reverse)
            return;

        forward = forward->next;
        reverse = reverse->prev;
    }

    if (!in_depth)
        quicklistCompressNode(node);

    /* At this point, forward and reverse are one node beyond depth */
    quicklistCompressNode(forward);
    quicklistCompressNode(reverse);
}

#define quicklistCompress(_ql, _node)                                          \
    do {                                                                       \
        if ((_node)->recompress)                                               \
            quicklistCompressNode((_node));                                    \
        else                                                                   \
            __quicklistCompress((_ql), (_node));                               \
    } while (0)

/* Insert 'new_node' after 'old_node' if 'after' is 1.
 * Insert 'new_node' before 'old_node' if 'after' is 0.
 * Note: 'new_node' is *always* uncompressed, so if we assign it to
 *       head or tail, we do not need to uncompress it. */
void __quicklist_insert_node(quicklist_t *quicklist,
                                        quicklist_node_t *old_node,
                                        quicklist_node_t *new_node, int after) {
    if (after) {
        new_node->prev = old_node;
        if (old_node) {
            new_node->next = old_node->next;
            if (old_node->next)
                old_node->next->prev = new_node;
            old_node->next = new_node;
        }
        if (quicklist->tail == old_node)
            quicklist->tail = new_node;
    } else {
        new_node->next = old_node;
        if (old_node) {
            new_node->prev = old_node->prev;
            if (old_node->prev)
                old_node->prev->next = new_node;
            old_node->prev = new_node;
        }
        if (quicklist->head == old_node)
            quicklist->head = new_node;
    }
    /* If this insert creates the only element so far, initialize head/tail. */
    if (quicklist->len == 0) {
        quicklist->head = quicklist->tail = new_node;
    }

    /* Update len first, so in __quicklistCompress we know exactly len */
    quicklist->len++;

    if (old_node)
        quicklistCompress(quicklist, old_node);

    quicklistCompress(quicklist, new_node);
}

static void __quicklistInsertPlainNode(quicklist_t *quicklist, quicklist_node_t *old_node,
                                       void *value, size_t sz, int after) {
    __quicklist_insert_node(quicklist, old_node, __quicklist_newPlainNode(value, sz), after);
    quicklist->count++;
}

/* Calculate the size limit or length limit of the quicklist node
 * based on 'fill', and is also used to limit list listpack. */
void quicklistNodeLimit(int fill, size_t *size, unsigned int *count) {
    *size = SIZE_MAX;
    *count = UINT_MAX;

    if (fill >= 0) {
        /* Ensure that one node have at least one entry */
        *count = (fill == 0) ? 1 : fill;
    } else {
        size_t offset = (-fill) - 1;
        size_t max_level = sizeof(optimization_level) / sizeof(*optimization_level);
        if (offset >= max_level) offset = max_level - 1;
        *size = optimization_level[offset];
    }
}


/* Wrappers for node inserting around existing node. */
void _quicklist_insert_nodeBefore(quicklist_t *quicklist,
                                             quicklist_node_t *old_node,
                                             quicklist_node_t *new_node) {
    __quicklist_insert_node(quicklist, old_node, new_node, 0);
}

void _quicklist_insert_nodeAfter(quicklist_t *quicklist,
                                            quicklist_node_t *old_node,
                                            quicklist_node_t *new_node) {
    __quicklist_insert_node(quicklist, old_node, new_node, 1);
}

#define sizeMeetsSafetyLimit(sz) ((sz) <= SIZE_SAFETY_LIMIT)

/* Check if the limit of the quicklist node has been reached to determine if
 * insertions, merges or other operations that would increase the size of
 * the node can be performed.
 * Return 1 if exceeds the limit, otherwise 0. */
int quicklistNodeExceedsLimit(int fill, size_t new_sz, unsigned int new_count) {
    size_t sz_limit;
    unsigned int count_limit;
    quicklistNodeLimit(fill, &sz_limit, &count_limit);

    if (likely(sz_limit != SIZE_MAX)) {
        return new_sz > sz_limit;
    } else if (count_limit != UINT_MAX) {
        /* when we reach here we know that the limit is a size limit (which is
         * safe, see comments next to optimization_level and SIZE_SAFETY_LIMIT) */
        if (!sizeMeetsSafetyLimit(new_sz)) return 1;
        return new_count > count_limit;
    }

    latte_unreachable();
}

int _quicklistNodeAllowInsert(const quicklist_node_t *node,
                                           const int fill, const size_t sz) {
    if (unlikely(!node))
        return 0;

    if (unlikely(QL_NODE_IS_PLAIN(node) || isLargeElement(sz)))
        return 0;

    /* Estimate how many bytes will be added to the listpack by this one entry.
     * We prefer an overestimation, which would at worse lead to a few bytes
     * below the lowest limit of 4k (see optimization_level).
     * Note: No need to check for overflow below since both `node->sz` and
     * `sz` are to be less than 1GB after the plain/large element check above. */
    size_t new_sz = node->sz + sz + SIZE_ESTIMATE_OVERHEAD;
    if (unlikely(quicklistNodeExceedsLimit(fill, new_sz, node->count + 1)))
        return 0;
    return 1;
}


struct quicklist_t *quicklist_new(void) {
    struct quicklist_t *quicklist;
    quicklist = zmalloc(sizeof(*quicklist));
    quicklist->head = quicklist->tail = NULL;
    quicklist->len = 0;
    quicklist->count = 0;
    quicklist->compress = 0;
    quicklist->fill = -2;
    quicklist->bookmark_count = 0;
    return quicklist;
}

struct quicklist_node_t *quicklist_newNode(void) {
    quicklist_node_t *node;
    node = zmalloc(sizeof(*node));
    node->entry = NULL;
    node->count = 0;
    node->sz = 0;
    node->next = node->prev = NULL;
    node->encoding = QUICKLIST_NODE_ENCODING_RAW;
    node->container = QUICKLIST_NODE_CONTAINER_PACKED;
    node->recompress= 0;
    node->dont_compress = 0;
    return node;
}


/*添加新条目到快速列表的头节点。
*
*如果使用现有的头返回0。
*如果创建了新的头部返回1。*/
int quicklistPushHead(quicklist_t *quicklist, void *value, size_t sz) {
    quicklist_node_t *orig_head = quicklist->head;

    if (unlikely(isLargeElement(sz))) {
        __quicklistInsertPlainNode(quicklist, quicklist->head, value, sz, 0);
        return 1;
    }

    if (likely(
            _quicklistNodeAllowInsert(quicklist->head, quicklist->fill, sz))) {
        quicklist->head->entry = lpPrepend(quicklist->head->entry, value, sz);
        quicklistNodeUpdateSz(quicklist->head);
    } else {
        quicklist_node_t *node = quicklist_newNode();
        node->entry = lpPrepend(lpNew(0), value, sz);

        quicklistNodeUpdateSz(node);
        _quicklist_insert_nodeBefore(quicklist, quicklist->head, node);
    }
    quicklist->count++;
    quicklist->head->count++;
    return (orig_head != quicklist->head);
}

/* Add new entry to tail node of quicklist.
 *
 * Returns 0 if used existing tail.
 * Returns 1 if new tail created. */
int quicklistPushTail(quicklist_t *quicklist, void *value, size_t sz) {
    quicklist_node_t *orig_tail = quicklist->tail;
    if (unlikely(isLargeElement(sz))) {
        __quicklistInsertPlainNode(quicklist, quicklist->tail, value, sz, 1);
        return 1;
    }

    if (likely(
            _quicklistNodeAllowInsert(quicklist->tail, quicklist->fill, sz))) {
        quicklist->tail->entry = lpAppend(quicklist->tail->entry, value, sz);
        quicklistNodeUpdateSz(quicklist->tail);
    } else {
        quicklist_node_t *node = quicklist_newNode();
        node->entry = lpAppend(lpNew(0), value, sz);

        quicklistNodeUpdateSz(node);
        _quicklist_insert_nodeAfter(quicklist, quicklist->tail, node);
    }
    quicklist->count++;
    quicklist->tail->count++;
    return (orig_tail != quicklist->tail);
}

void quicklistPush(quicklist_t *quicklist, void *value, const size_t sz,
                   int where) {
    if (quicklist->head) {
        assert(quicklist->head->encoding != QUICKLIST_NODE_ENCODING_LZF);
    }

    if (quicklist->tail) {
        assert(quicklist->tail->encoding != QUICKLIST_NODE_ENCODING_LZF);
    }
 
    if (where = QUICKLIST_HEAD) {
        quicklistPushHead(quicklist, value, sz);
    } else {
        quicklistPushTail(quicklist, value, sz);
    }
}

/* If we previously used quicklistDecompressNodeForUse(), just recompress. */
#define quicklistRecompressOnly(_node)                                         \
    do {                                                                       \
        if ((_node)->recompress)                                               \
            quicklistCompressNode((_node));                                    \
    } while (0)

void quicklistRepr(unsigned  char* ql, int full) {
    int i = 0;
    quicklist_t *quicklist = (struct quicklist_t*) ql;
    printf("{count : %ld}\n", quicklist->count);
    printf("{len: %ld}\n", quicklist->len);
    printf("{fill: %d}\n", quicklist->fill);
    printf("{compress : %d}\n", quicklist->compress);
    printf("{bookmark_count : %d}\n", quicklist->bookmark_count);
    quicklist_node_t* node = quicklist->head;

    while(node != NULL) {
        printf("{quicklist node(%d)\n", i++);
        printf("{container : %s, encoding: %s, size: %zu, count: %d, recompress: %d, attempted_compress: %d}\n",
               QL_NODE_IS_PLAIN(node) ? "PLAIN": "PACKED",
               (node->encoding == QUICKLIST_NODE_ENCODING_RAW) ? "RAW": "LZF",
               node->sz,
               node->count,
               node->recompress,
               node->attempted_compress);

        if (full) {
            quicklistDecompressNode(node);
            if (node->container == QUICKLIST_NODE_CONTAINER_PACKED) {
                printf("{ listpack:\n");
                lpRepr(node->entry);
                printf("}\n");

            } else if (QL_NODE_IS_PLAIN(node)) {
                printf("{ entry : %s }\n", node->entry);
            }
            printf("}\n");
            quicklistRecompressOnly(node);
        }
        node = node->next;
    }

}

