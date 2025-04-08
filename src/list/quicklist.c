#include <stdio.h>
#include "quicklist.h"
#include "utils/utils.h"
#include <assert.h>
#include <limits.h>
#include "listpack.h"
#include <string.h>
#include <stdlib.h>
#include "lzf/lzfP.h"

/* packed_threshold is initialized to 1gb*/
static size_t packed_threshold = (1 << 30);

/* set threshold for PLAIN nodes, the real limit is 4gb */
#define isLargeElement(size) ((size) >= packed_threshold)

#define SIZE_ESTIMATE_OVERHEAD 8

/* Minimum listpack size in bytes for attempting compression. */
#define MIN_COMPRESS_BYTES 48
// int quick_listisSetPackedThreshold(size_t sz) {
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

/* Minimum size reduction in bytes to store compressed quick_listNode data.
 * This also prevents us from storing compression if the compression
 * resulted in a larger size than the original data. */
#define MIN_COMPRESS_IMPROVE 8

/* Optimization levels for size-based filling.
 * Note that the largest possible limit is 64k, so even if each record takes
 * just one byte, it still won't overflow the 16 bit count field. */
static const size_t optimization_level[] = {4096, 8192, 16384, 32768, 65536};

#define quick_listNodeUpdateSz(node)                                            \
    do {                                                                       \
        (node)->sz = lp_bytes((node)->entry);                                   \
    } while (0)

static quick_list_node_t* __quick_list_newPlainNode(void *value, size_t sz) {
    quick_list_node_t *new_node = quick_list_new_node();
    new_node->entry = zmalloc(sz);
    new_node->container = quick_list_NODE_CONTAINER_PLAIN;
    memcpy(new_node->entry, value, sz);
    new_node->sz = sz;
    new_node->count++;
    return new_node;
}

/* Compress the listpack in 'node' and update encoding details.
 * Returns 1 if listpack compressed successfully.
 * Returns 0 if compression failed or if listpack too small to compress. */
int __quick_listCompressNode(quick_list_node_t *node) {
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

    quick_list_LZF_t *lzf = zmalloc(sizeof(*lzf) + node->sz);

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
    node->encoding = quick_list_NODE_ENCODING_LZF;
    return 1;
}

/* Compress only uncompressed nodes. */
#define quick_listCompressNode(_node)                                           \
    do {                                                                       \
        if ((_node) && (_node)->encoding == quick_list_NODE_ENCODING_RAW) {     \
            __quick_listCompressNode((_node));                                  \
        }                                                                      \
    } while (0)


#define quick_listAllowsCompression(_ql) ((_ql)->compress != 0)

/* Uncompress the listpack in 'node' and update encoding details.
 * Returns 1 on successful decode, 0 on failure to decode. */
int __quick_listDecompressNode(quick_list_node_t *node) {
#ifdef REDIS_TEST
    node->attempted_compress = 0;
#endif
    node->recompress = 0;

    void *decompressed = zmalloc(node->sz);
    quick_list_LZF_t *lzf = (quick_list_LZF_t *)node->entry;
    if (lzf_decompress(lzf->compressed, lzf->sz, decompressed, node->sz) == 0) {
        /* Someone requested decompress, but we can't decompress.  Not good. */
        zfree(decompressed);
        return 0;
    }
    zfree(lzf);
    node->entry = decompressed;
    node->encoding = quick_list_NODE_ENCODING_RAW;
    return 1;
}
/* Decompress only compressed nodes. */
#define quick_listDecompressNode(_node)                                         \
    do {                                                                       \
        if ((_node) && (_node)->encoding == quick_list_NODE_ENCODING_LZF) {     \
            __quick_listDecompressNode((_node));                                \
        }                                                                      \
    } while (0)
/* Force 'quick_list' to meet compression guidelines set by compress depth.
 * The only way to guarantee interior nodes get compressed is to iterate
 * to our "interior" compress depth then compress the next node we find.
 * If compress depth is larger than the entire list, we return immediately. */
void __quick_listCompress(const quick_list_t *quick_list,
                                      quick_list_node_t *node) {
    if (quick_list->len == 0) return;

    /* The head and tail should never be compressed (we should not attempt to recompress them) */
    assert(quick_list->head->recompress == 0 && quick_list->tail->recompress == 0);

    /* If length is less than our compress depth (from both sides),
     * we can't compress anything. */
    if (!quick_listAllowsCompression(quick_list) ||
        quick_list->len < (unsigned int)(quick_list->compress * 2))
        return;

#if 0
    /* Optimized cases for small depth counts */
    if (quick_list->compress == 1) {
        quick_list_node_t *h = quick_list->head, *t = quick_list->tail;
        quick_listDecompressNode(h);
        quick_listDecompressNode(t);
        if (h != node && t != node)
            quick_listCompressNode(node);
        return;
    } else if (quick_list->compress == 2) {
        quick_list_node_t *h = quick_list->head, *hn = h->next, *hnn = hn->next;
        quick_list_node_t *t = quick_list->tail, *tp = t->prev, *tpp = tp->prev;
        quick_listDecompressNode(h);
        quick_listDecompressNode(hn);
        quick_listDecompressNode(t);
        quick_listDecompressNode(tp);
        if (h != node && hn != node && t != node && tp != node) {
            quick_listCompressNode(node);
        }
        if (hnn != t) {
            quick_listCompressNode(hnn);
        }
        if (tpp != h) {
            quick_listCompressNode(tpp);
        }
        return;
    }
#endif

    /* Iterate until we reach compress depth for both sides of the list.a
     * Note: because we do length checks at the *top* of this function,
     *       we can skip explicit null checks below. Everything exists. */
    quick_list_node_t *forward = quick_list->head;
    quick_list_node_t *reverse = quick_list->tail;
    int depth = 0;
    int in_depth = 0;
    while (depth++ < quick_list->compress) {
        quick_listDecompressNode(forward);
        quick_listDecompressNode(reverse);

        if (forward == node || reverse == node)
            in_depth = 1;

        /* We passed into compress depth of opposite side of the quick_list
         * so there's no need to compress anything and we can exit. */
        if (forward == reverse || forward->next == reverse)
            return;

        forward = forward->next;
        reverse = reverse->prev;
    }

    if (!in_depth)
        quick_listCompressNode(node);

    /* At this point, forward and reverse are one node beyond depth */
    quick_listCompressNode(forward);
    quick_listCompressNode(reverse);
}

#define quick_listCompress(_ql, _node)                                          \
    do {                                                                       \
        if ((_node)->recompress)                                               \
            quick_listCompressNode((_node));                                    \
        else                                                                   \
            __quick_listCompress((_ql), (_node));                               \
    } while (0)

/* Insert 'new_node' after 'old_node' if 'after' is 1.
 * Insert 'new_node' before 'old_node' if 'after' is 0.
 * Note: 'new_node' is *always* uncompressed, so if we assign it to
 *       head or tail, we do not need to uncompress it. */
void __quick_list_insert_node(quick_list_t *quick_list,
                                        quick_list_node_t *old_node,
                                        quick_list_node_t *new_node, int after) {
    if (after) {
        new_node->prev = old_node;
        if (old_node) {
            new_node->next = old_node->next;
            if (old_node->next)
                old_node->next->prev = new_node;
            old_node->next = new_node;
        }
        if (quick_list->tail == old_node)
            quick_list->tail = new_node;
    } else {
        new_node->next = old_node;
        if (old_node) {
            new_node->prev = old_node->prev;
            if (old_node->prev)
                old_node->prev->next = new_node;
            old_node->prev = new_node;
        }
        if (quick_list->head == old_node)
            quick_list->head = new_node;
    }
    /* If this insert creates the only element so far, initialize head/tail. */
    if (quick_list->len == 0) {
        quick_list->head = quick_list->tail = new_node;
    }

    /* Update len first, so in __quick_listCompress we know exactly len */
    quick_list->len++;

    if (old_node)
        quick_listCompress(quick_list, old_node);

    quick_listCompress(quick_list, new_node);
}

static void __quick_listInsertPlainNode(quick_list_t *quick_list, quick_list_node_t *old_node,
                                       void *value, size_t sz, int after) {
    __quick_list_insert_node(quick_list, old_node, __quick_list_newPlainNode(value, sz), after);
    quick_list->count++;
}

/* Calculate the size limit or length limit of the quick_list node
 * based on 'fill', and is also used to limit list listpack. */
void quick_listNodeLimit(int fill, size_t *size, unsigned int *count) {
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
void _quick_list_insert_nodeBefore(quick_list_t *quick_list,
                                             quick_list_node_t *old_node,
                                             quick_list_node_t *new_node) {
    __quick_list_insert_node(quick_list, old_node, new_node, 0);
}

void _quick_list_insert_nodeAfter(quick_list_t *quick_list,
                                            quick_list_node_t *old_node,
                                            quick_list_node_t *new_node) {
    __quick_list_insert_node(quick_list, old_node, new_node, 1);
}

#define sizeMeetsSafetyLimit(sz) ((sz) <= SIZE_SAFETY_LIMIT)

/* Check if the limit of the quick_list node has been reached to determine if
 * insertions, merges or other operations that would increase the size of
 * the node can be performed.
 * Return 1 if exceeds the limit, otherwise 0. */
int quick_listNodeExceedsLimit(int fill, size_t new_sz, unsigned int new_count) {
    size_t sz_limit;
    unsigned int count_limit;
    quick_listNodeLimit(fill, &sz_limit, &count_limit);

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

int _quick_listNodeAllowInsert(const quick_list_node_t *node,
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
    if (unlikely(quick_listNodeExceedsLimit(fill, new_sz, node->count + 1)))
        return 0;
    return 1;
}


struct quick_list_t *quick_list_create(void) {
    struct quick_list_t *quick_list;
    quick_list = zmalloc(sizeof(*quick_list));
    quick_list->head = quick_list->tail = NULL;
    quick_list->len = 0;
    quick_list->count = 0;
    quick_list->compress = 0;
    quick_list->fill = -2;
    quick_list->bookmark_count = 0;
    return quick_list;
}

#define FILL_MAX ((1 << (QL_FILL_BITS-1))-1)
void quick_list_set_fill(quick_list_t *quicklist, int fill) {
    if (fill > FILL_MAX) {
        fill = FILL_MAX;
    } else if (fill < -5) {
        fill = -5;
    }
    quicklist->fill = fill;
}

#define COMPRESS_MAX ((1 << QL_COMP_BITS)-1)
void quick_list_set_comress_depth(quick_list_t *quicklist, int compress) {
    if (compress > COMPRESS_MAX) {
        compress = COMPRESS_MAX;
    } else if (compress < 0) {
        compress = 0;
    }
    quicklist->compress = compress;
}

void quick_list_set_options(quick_list_t* quick_list, int fill, int compress) {
    quick_list_set_fill(quick_list, fill);
    quick_list_set_comress_depth(quick_list, compress);
}

quick_list_t* quick_list_new(int fill, int compress) {
    quick_list_t* quick_list = quick_list_create();
    quick_list_set_options(quick_list, fill, compress);
    return quick_list;
}

struct quick_list_node_t *quick_list_new_node(void) {
    quick_list_node_t *node;
    node = zmalloc(sizeof(*node));
    node->entry = NULL;
    node->count = 0;
    node->sz = 0;
    node->next = node->prev = NULL;
    node->encoding = quick_list_NODE_ENCODING_RAW;
    node->container = quick_list_NODE_CONTAINER_PACKED;
    node->recompress= 0;
    node->dont_compress = 0;
    return node;
}


/*添加新条目到快速列表的头节点。
*
*如果使用现有的头返回0。
*如果创建了新的头部返回1。*/
int quick_list_push_head(quick_list_t *quick_list, void *value, size_t sz) {
    quick_list_node_t *orig_head = quick_list->head;

    if (unlikely(isLargeElement(sz))) {
        __quick_listInsertPlainNode(quick_list, quick_list->head, value, sz, 0);
        return 1;
    }

    if (likely(
            _quick_listNodeAllowInsert(quick_list->head, quick_list->fill, sz))) {
        quick_list->head->entry = lp_prepend(quick_list->head->entry, value, sz);
        quick_listNodeUpdateSz(quick_list->head);
    } else {
        quick_list_node_t *node = quick_list_new_node();
        node->entry = lp_prepend(lp_new(0), value, sz);

        quick_listNodeUpdateSz(node);
        _quick_list_insert_nodeBefore(quick_list, quick_list->head, node);
    }
    quick_list->count++;
    quick_list->head->count++;
    return (orig_head != quick_list->head);
}

/* Add new entry to tail node of quick_list.
 *
 * Returns 0 if used existing tail.
 * Returns 1 if new tail created. */
int quick_list_push_tail(quick_list_t *quick_list, void *value, size_t sz) {
    quick_list_node_t *orig_tail = quick_list->tail;
    if (unlikely(isLargeElement(sz))) {
        __quick_listInsertPlainNode(quick_list, quick_list->tail, value, sz, 1);
        return 1;
    }

    if (likely(
            _quick_listNodeAllowInsert(quick_list->tail, quick_list->fill, sz))) {
        quick_list->tail->entry = lp_append(quick_list->tail->entry, value, sz);
        quick_listNodeUpdateSz(quick_list->tail);
    } else {
        quick_list_node_t *node = quick_list_new_node();
        node->entry = lp_append(lp_new(0), value, sz);

        quick_listNodeUpdateSz(node);
        _quick_list_insert_nodeAfter(quick_list, quick_list->tail, node);
    }
    quick_list->count++;
    quick_list->tail->count++;
    return (orig_tail != quick_list->tail);
}

void quick_list_push(quick_list_t *quick_list, void *value, const size_t sz,
                   int where) {
    if (quick_list->head) {
        assert(quick_list->head->encoding != quick_list_NODE_ENCODING_LZF);
    }

    if (quick_list->tail) {
        assert(quick_list->tail->encoding != quick_list_NODE_ENCODING_LZF);
    }
 
    if (where == quick_list_HEAD) {
        quick_list_push_head(quick_list, value, sz);
    } else {
        quick_list_push_tail(quick_list, value, sz);
    }
}

/* If we previously used quick_listDecompressNodeForUse(), just recompress. */
#define quick_listRecompressOnly(_node)                                         \
    do {                                                                       \
        if ((_node)->recompress)                                               \
            quick_listCompressNode((_node));                                    \
    } while (0)

void quick_list_repr(unsigned  char* ql, int full) {
    int i = 0;
    quick_list_t *quick_list = (struct quick_list_t*) ql;
    printf("{count : %ld}\n", quick_list->count);
    printf("{len: %ld}\n", quick_list->len);
    printf("{fill: %d}\n", quick_list->fill);
    printf("{compress : %d}\n", quick_list->compress);
    printf("{bookmark_count : %d}\n", quick_list->bookmark_count);
    quick_list_node_t* node = quick_list->head;

    while(node != NULL) {
        printf("{quick_list node(%d)\n", i++);
        printf("{container : %s, encoding: %s, size: %zu, count: %d, recompress: %d, attempted_compress: %d}\n",
               QL_NODE_IS_PLAIN(node) ? "PLAIN": "PACKED",
               (node->encoding == quick_list_NODE_ENCODING_RAW) ? "RAW": "LZF",
               node->sz,
               node->count,
               node->recompress,
               node->attempted_compress);

        if (full) {
            quick_listDecompressNode(node);
            if (node->container == quick_list_NODE_CONTAINER_PACKED) {
                printf("{ listpack:\n");
                lp_repr(node->entry);
                printf("}\n");

            } else if (QL_NODE_IS_PLAIN(node)) {
                printf("{ entry : %s }\n", node->entry);
            }
            printf("}\n");
            quick_listRecompressOnly(node);
        }
        node = node->next;
    }

}

void quick_list_book_mark_clear(quick_list_t* ql) {
    while (ql->bookmark_count)
        zfree(ql->bookmarks[--ql->bookmark_count].name);
    /* NOTE: We do not shrink (realloc) the quick list. main use case for this
     * function is just before releasing the allocation. */
}

void quick_list_release(quick_list_t* quick_list) {
    unsigned long len;
    quick_list_node_t* current, *next;

    current = quick_list->head;
    len = quick_list->len;
    while (len--) {
        next = current->next;
        zfree(current->entry);
        quick_list->count -= current->count;
        zfree(current);
        quick_list->len--;
        current = next;
    }
    quick_list_book_mark_clear(quick_list);
    zfree(quick_list);
}
