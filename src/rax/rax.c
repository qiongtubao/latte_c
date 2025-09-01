#include "rax.h"
#include "zmalloc/zmalloc.h"
#include "utils/utils.h"
#include <string.h>
#include <errno.h>

#define RAX_NODE_MAX_SIZE ((1<<29)-1)

/* 计算在一个大小为 'nodesize' 的节点中，其“字符部分”所需的填充字节数。
 * 填充的目的是为了保证存储在其中的子节点指针地址是内存对齐的。
 * 注意：我们在节点大小上加了4，因为节点本身包含一个4字节的头部。 */
#define rax_padding(nodesize) ((sizeof(void*)-(((nodesize)+4) % sizeof(void*))) & (sizeof(void*)-1))

rax_node_t* rax_new_node(int children, int data_field) {
    size_t node_size = sizeof(rax_node_t) + children + 
        rax_padding(children) +
        sizeof(rax_node_t*) * children;
    if (data_field) node_size += sizeof(void*);
    rax_node_t* node = (rax_node_t*)zmalloc(node_size);
    if (node == NULL) {
        return NULL;
    }
    node->is_key = 0;
    node->is_null = 0;
    node->is_compr = 0;
    node->size = children;
    return node;
}


rax_t* rax_new_with_meta(int meta_size) {
    rax_t* rax = (rax_t*)zmalloc(sizeof(rax_t) + meta_size);
    if (rax == NULL) {
        return NULL;
    }
    rax->num_ele = 0;
    rax->num_nodes = 1;
    rax->head = rax_new_node(0, 0);
    if (rax->head == NULL) {
        rax_delete(rax);
        return NULL;
    }    
    return rax;
}

rax_t* rax_new() {
    return rax_new_with_meta(0);
}


/* 计算节点当前的总大小。注意：第二行计算了在字符序列之后所需的填充字节，
 * 这是为了保证后续存储的子节点指针地址是内存对齐的。 */
#define rax_node_current_length(n) ( \
    sizeof(rax_node_t)+(n)->size+ \
    rax_padding((n)->size)+ \
    ((n)->is_compr ? sizeof(rax_node_t*) : sizeof(rax_node_t*)*(n)->size)+ \
    (((n)->is_key && !(n)->is_null)*sizeof(void*)) \
)

/* 返回指向节点中最后一个子节点指针的地址。
 * 对于压缩节点，这既是最后一个，也是唯一的子节点指针。 */
#define rax_node_last_child_ptr(n) ((rax_node_t**) ( \
    ((char*)(n)) + \
    rax_node_current_length(n) - \
    sizeof(rax_node_t*) - \
    (((n)->is_key && !(n)->is_null) ? sizeof(void*) : 0) \
))

/* 返回指向节点中第一个子节点指针的地址。 */
#define rax_node_first_child_ptr(n) ((rax_node_t**) ( \
    (n)->data + \
    (n)->size + \
    rax_padding((n)->size)))

/* Get the node auxiliary data. */
void *rax_get_data(rax_node_t *n) {
    if (n->is_null) return NULL;
    void **ndata =(void**)((char*)n+rax_node_current_length(n)-sizeof(void*));
    void *data;
    memcpy(&data,ndata,sizeof(data));
    return data;
}

/* Set the node auxiliary data to the specified pointer. */
void rax_set_data(rax_node_t *n, void *data) {
    n->is_key = 1;
    if (data != NULL) {
        n->is_null = 0;
        void **ndata = (void**)
            ((char*)n+rax_node_current_length(n)-sizeof(void*));
        memcpy(ndata,&data,sizeof(data));
    } else {
        n->is_null = 1;
    }
}

/* realloc the node to make room for auxiliary data in order
 * to store an item in that node. On out of memory NULL is returned. */
rax_node_t *rax_realloc_for_data(rax_node_t *n, void *data) {
    if (data == NULL) return n; /* No reallocation needed, setting isnull=1 */
    size_t curlen = rax_node_current_length(n);
    return (rax_node_t *)zrealloc(n,curlen+sizeof(void*));
}

void rax_recursive_delete(rax_t* rax, rax_node_t* node, void (*free_callback)(void*)) {
    // debug_node("free tracersing", node);
    int num_children = node->is_compr? 1: node->size;
    rax_node_t ** cp = rax_node_last_child_ptr(node);
    while (num_children--) {
        rax_node_t* child;
        memcpy(&child, cp, sizeof(child));
        rax_recursive_delete(rax, *cp, free_callback);
        cp--;
    }
    // debug_node("free depth-first", node);
    if (free_callback && node->is_key && node->is_null) {
        free_callback(rax_get_data(node));
    }
    zfree(node);
    rax->num_nodes--;
}

void raw_delete_with_callback(rax_t* rax, void (*free_callback)(void*)) {
    rax_recursive_delete(rax, rax->head, free_callback);
    latte_assert_with_info(rax->num_nodes == 0, "rax_delete_callback: num_nodes is not 0");
    zfree(rax);
}

void rax_delete(rax_t* rax) {
    raw_delete_with_callback(rax, NULL);
}

/* Turn the node 'n', that must be a node without any children, into a
 * compressed node representing a set of nodes linked one after the other
 * and having exactly one child each. The node can be a key or not: this
 * property and the associated value if any will be preserved.
 *
 * The function also returns a child node, since the last node of the
 * compressed chain cannot be part of the chain: it has zero children while
 * we can only compress inner nodes with exactly one child each. */
rax_node_t *rax_compress_node(rax_node_t *n, unsigned char *s, size_t len, rax_node_t **child) {
    latte_assert_with_info(n->size == 0 && n->is_compr == 0, "rax_compress_node: node is not a leaf node");
    void *data = NULL; /* Initialized only to avoid warnings. */
    size_t newsize;

    // debugf("Compress node: %.*s\n", (int)len,s);

    /* Allocate the child to link to this node. */
    *child = rax_new_node(0,0);
    if (*child == NULL) return NULL;

    /* Make space in the parent node. */
    newsize = sizeof(rax_node_t)+len+rax_padding(len)+sizeof(rax_node_t*);
    if (n->is_key) {
        data = rax_get_data(n); /* To restore it later. */
        if (!n->is_null) newsize += sizeof(void*);
    }
    rax_node_t *newn = (rax_node_t *)zrealloc(n,newsize);
    if (newn == NULL) {
        zfree(*child);
        return NULL;
    }
    n = newn;

    n->is_compr = 1;
    n->size = len;
    memcpy(n->data,s,len);
    if (n->is_key) rax_set_data(n,data);
    rax_node_t **childfield = rax_node_last_child_ptr(n);
    memcpy(childfield,child,sizeof(*child));
    return n;
}

/* Add a new child to the node 'n' representing the character 'c' and return
 * its new pointer, as well as the child pointer by reference. Additionally
 * '***parentlink' is populated with the raxNode pointer-to-pointer of where
 * the new child was stored, which is useful for the caller to replace the
 * child pointer if it gets reallocated.
 *
 * On success the new parent node pointer is returned (it may change because
 * of the realloc, so the caller should discard 'n' and use the new value).
 * On out of memory NULL is returned, and the old node is still valid. */
rax_node_t *rax_add_child(rax_node_t *n, unsigned char c, rax_node_t **childptr, rax_node_t ***parentlink) {
    latte_assert_with_info(n->is_compr == 0, "rax_add_child: node is a compressed node");

    size_t curlen = rax_node_current_length(n);
    n->size++;
    size_t newlen = rax_node_current_length(n);
    n->size--; /* For now restore the original size. We'll update it only on
                  success at the end. */

    /* Alloc the new child we will link to 'n'. */
    rax_node_t *child = rax_new_node(0,0);
    if (child == NULL) return NULL;

    /* Make space in the original node. */
    rax_node_t *newn = (rax_node_t *)zrealloc(n,newlen);
    if (newn == NULL) {
        zfree(child);
        return NULL;
    }
    n = newn;

    /* After the reallocation, we have up to 8/16 (depending on the system
     * pointer size, and the required node padding) bytes at the end, that is,
     * the additional char in the 'data' section, plus one pointer to the new
     * child, plus the padding needed in order to store addresses into aligned
     * locations.
     *
     * So if we start with the following node, having "abde" edges.
     *
     * Note:
     * - We assume 4 bytes pointer for simplicity.
     * - Each space below corresponds to one byte
     *
     * [HDR*][abde][Aptr][Bptr][Dptr][Eptr]|AUXP|
     *
     * After the reallocation we need: 1 byte for the new edge character
     * plus 4 bytes for a new child pointer (assuming 32 bit machine).
     * However after adding 1 byte to the edge char, the header + the edge
     * characters are no longer aligned, so we also need 3 bytes of padding.
     * In total the reallocation will add 1+4+3 bytes = 8 bytes:
     *
     * (Blank bytes are represented by ".")
     *
     * [HDR*][abde][Aptr][Bptr][Dptr][Eptr]|AUXP|[....][....]
     *
     * Let's find where to insert the new child in order to make sure
     * it is inserted in-place lexicographically. Assuming we are adding
     * a child "c" in our case pos will be = 2 after the end of the following
     * loop. */
    int pos;
    for (pos = 0; pos < n->size; pos++) {
        if (n->data[pos] > c) break;
    }

    /* Now, if present, move auxiliary data pointer at the end
     * so that we can mess with the other data without overwriting it.
     * We will obtain something like that:
     *
     * [HDR*][abde][Aptr][Bptr][Dptr][Eptr][....][....]|AUXP|
     */
    unsigned char *src, *dst;
    if (n->is_key && !n->is_null) {
        src = ((unsigned char*)n+curlen-sizeof(void*));
        dst = ((unsigned char*)n+newlen-sizeof(void*));
        memmove(dst,src,sizeof(void*));
    }

    /* Compute the "shift", that is, how many bytes we need to move the
     * pointers section forward because of the addition of the new child
     * byte in the string section. Note that if we had no padding, that
     * would be always "1", since we are adding a single byte in the string
     * section of the node (where now there is "abde" basically).
     *
     * However we have padding, so it could be zero, or up to 8.
     *
     * Another way to think at the shift is, how many bytes we need to
     * move child pointers forward *other than* the obvious sizeof(void*)
     * needed for the additional pointer itself. */
    size_t shift = newlen - curlen - sizeof(void*);

    /* We said we are adding a node with edge 'c'. The insertion
     * point is between 'b' and 'd', so the 'pos' variable value is
     * the index of the first child pointer that we need to move forward
     * to make space for our new pointer.
     *
     * To start, move all the child pointers after the insertion point
     * of shift+sizeof(pointer) bytes on the right, to obtain:
     *
     * [HDR*][abde][Aptr][Bptr][....][....][Dptr][Eptr]|AUXP|
     */
    src = n->data+n->size+
          rax_padding(n->size)+
          sizeof(rax_node_t*)*pos;
    memmove(src+shift+sizeof(rax_node_t*),src,sizeof(rax_node_t*)*(n->size-pos));

    /* Move the pointers to the left of the insertion position as well. Often
     * we don't need to do anything if there was already some padding to use. In
     * that case the final destination of the pointers will be the same, however
     * in our example there was no pre-existing padding, so we added one byte
     * plus three bytes of padding. After the next memmove() things will look
     * like that:
     *
     * [HDR*][abde][....][Aptr][Bptr][....][Dptr][Eptr]|AUXP|
     */
    if (shift) {
        src = (unsigned char*) rax_node_first_child_ptr(n);
        memmove(src+shift,src,sizeof(rax_node_t*)*pos);
    }

    /* Now make the space for the additional char in the data section,
     * but also move the pointers before the insertion point to the right
     * by shift bytes, in order to obtain the following:
     *
     * [HDR*][ab.d][e...][Aptr][Bptr][....][Dptr][Eptr]|AUXP|
     */
    src = n->data+pos;
    memmove(src+1,src,n->size-pos);

    /* We can now set the character and its child node pointer to get:
     *
     * [HDR*][abcd][e...][Aptr][Bptr][....][Dptr][Eptr]|AUXP|
     * [HDR*][abcd][e...][Aptr][Bptr][Cptr][Dptr][Eptr]|AUXP|
     */
    n->data[pos] = c;
    n->size++;
    src = (unsigned char*) rax_node_first_child_ptr(n);
    rax_node_t **childfield = (rax_node_t**)(src+sizeof(rax_node_t*)*pos);
    memcpy(childfield,&child,sizeof(child));
    *childptr = child;
    *parentlink = childfield;
    return n;
}

/* Push an item into the stack, returns 1 on success, 0 on out of memory. */
static inline int rax_stack_push(rax_stack_t *ts, void *ptr) {
    if (ts->items == ts->max_items) {
        if (ts->stack == ts->static_items) {
            ts->stack = zmalloc(sizeof(void*)*ts->max_items*2);
            if (ts->stack == NULL) {
                ts->stack = ts->static_items;
                ts->oom = 1;
                errno = ENOMEM;
                return 0;
            }
            memcpy(ts->stack,ts->static_items,sizeof(void*)*ts->max_items);
        } else {
            void **newalloc = zrealloc(ts->stack,sizeof(void*)*ts->max_items*2);
            if (newalloc == NULL) {
                ts->oom = 1;
                errno = ENOMEM;
                return 0;
            }
            ts->stack = newalloc;
        }
        ts->max_items *= 2;
    }
    ts->stack[ts->items] = ptr;
    ts->items++;
    return 1;
}



/* Low level function that walks the tree looking for the string
 * 's' of 'len' bytes. The function returns the number of characters
 * of the key that was possible to process: if the returned integer
 * is the same as 'len', then it means that the node corresponding to the
 * string was found (however it may not be a key in case the node->iskey is
 * zero or if simply we stopped in the middle of a compressed node, so that
 * 'splitpos' is non zero).
 *
 * Otherwise if the returned integer is not the same as 'len', there was an
 * early stop during the tree walk because of a character mismatch.
 *
 * The node where the search ended (because the full string was processed
 * or because there was an early stop) is returned by reference as
 * '*stopnode' if the passed pointer is not NULL. This node link in the
 * parent's node is returned as '*plink' if not NULL. Finally, if the
 * search stopped in a compressed node, '*splitpos' returns the index
 * inside the compressed node where the search ended. This is useful to
 * know where to split the node for insertion.
 *
 * Note that when we stop in the middle of a compressed node with
 * a perfect match, this function will return a length equal to the
 * 'len' argument (all the key matched), and will return a *splitpos which is
 * always positive (that will represent the index of the character immediately
 * *after* the last match in the current compressed node).
 *
 * When instead we stop at a compressed node and *splitpos is zero, it
 * means that the current node represents the key (that is, none of the
 * compressed node characters are needed to represent the key, just all
 * its parents nodes). */
static inline size_t rax_low_walk(rax_t *rax, unsigned char *s, size_t len, rax_node_t **stopnode, rax_node_t ***plink, int *splitpos, rax_stack_t *ts) {
    rax_node_t *h = rax->head;
    rax_node_t **parentlink = &rax->head;

    size_t i = 0; /* Position in the string. */
    size_t j = 0; /* Position in the node children (or bytes if compressed).*/
    while(h->size && i < len) {
        // debug_node("Lookup current node",h);
        unsigned char *v = h->data;

        if (h->is_compr) {
            for (j = 0; j < h->size && i < len; j++, i++) {
                if (v[j] != s[i]) break;
            }
            if (j != h->size) break;
        } else {
            /* Even when h->size is large, linear scan provides good
             * performances compared to other approaches that are in theory
             * more sounding, like performing a binary search. */
            for (j = 0; j < h->size; j++) {
                if (v[j] == s[i]) break;
            }
            if (j == h->size) break;
            i++;
        }

        if (ts) rax_stack_push(ts,h); /* Save stack of parent nodes. */
        rax_node_t **children = rax_node_first_child_ptr(h);
        if (h->is_compr) j = 0; /* Compressed node only child is at index 0. */
        memcpy(&h,children+j,sizeof(h));
        parentlink = children+j;
        j = 0; /* If the new node is non compressed and we do not
                  iterate again (since i == len) set the split
                  position to 0 to signal this node represents
                  the searched key. */
    }
    // debug_node("Lookup stop node is",h);
    if (stopnode) *stopnode = h;
    if (plink) *plink = parentlink;
    if (splitpos && h->is_compr) *splitpos = j;
    return i;
}

/* Insert the element 's' of size 'len', setting as auxiliary data
 * the pointer 'data'. If the element is already present, the associated
 * data is updated (only if 'overwrite' is set to 1), and 0 is returned,
 * otherwise the element is inserted and 1 is returned. On out of memory the
 * function returns 0 as well but sets errno to ENOMEM, otherwise errno will
 * be set to 0.
 */
int rax_generic_insert(rax_t *rax, unsigned char *s, size_t len, void *data, void **old, int overwrite) {
    size_t i;
    int j = 0; /* Split position. If raxLowWalk() stops in a compressed
                  node, the index 'j' represents the char we stopped within the
                  compressed node, that is, the position where to split the
                  node for insertion. */
    rax_node_t *h, **parentlink;

    // debugf("### Insert %.*s with value %p\n", (int)len, s, data);
    i = rax_low_walk(rax,s,len,&h,&parentlink,&j,NULL);

    /* If i == len we walked following the whole string. If we are not
     * in the middle of a compressed node, the string is either already
     * inserted or this middle node is currently not a key, but can represent
     * our key. We have just to reallocate the node and make space for the
     * data pointer. */
    if (i == len && (!h->is_compr || j == 0 /* not in the middle if j is 0 */)) {
        // debugf("### Insert: node representing key exists\n");
        /* Make space for the value pointer if needed. */
        if (!h->is_key || (h->is_null && overwrite)) {
            h = rax_realloc_for_data(h,data);
            if (h) memcpy(parentlink,&h,sizeof(h));
        }
        if (h == NULL) {
            errno = ENOMEM;
            return 0;
        }

        /* Update the existing key if there is already one. */
        if (h->is_key) {
            if (old) *old = rax_get_data(h);
            if (overwrite) rax_set_data(h,data);
            errno = 0;
            return 0; /* Element already exists. */
        }

        /* Otherwise set the node as a key. Note that raxSetData()
         * will set h->iskey. */
        rax_set_data(h,data);
        rax->num_ele++;
        return 1; /* Element inserted. */
    }

    /* If the node we stopped at is a compressed node, we need to
     * split it before to continue.
     *
     * Splitting a compressed node have a few possible cases.
     * Imagine that the node 'h' we are currently at is a compressed
     * node containing the string "ANNIBALE" (it means that it represents
     * nodes A -> N -> N -> I -> B -> A -> L -> E with the only child
     * pointer of this node pointing at the 'E' node, because remember that
     * we have characters at the edges of the graph, not inside the nodes
     * themselves.
     *
     * In order to show a real case imagine our node to also point to
     * another compressed node, that finally points at the node without
     * children, representing 'O':
     *
     *     "ANNIBALE" -> "SCO" -> []
     *
     * When inserting we may face the following cases. Note that all the cases
     * require the insertion of a non compressed node with exactly two
     * children, except for the last case which just requires splitting a
     * compressed node.
     *
     * 1) Inserting "ANNIENTARE"
     *
     *               |B| -> "ALE" -> "SCO" -> []
     *     "ANNI" -> |-|
     *               |E| -> (... continue algo ...) "NTARE" -> []
     *
     * 2) Inserting "ANNIBALI"
     *
     *                  |E| -> "SCO" -> []
     *     "ANNIBAL" -> |-|
     *                  |I| -> (... continue algo ...) []
     *
     * 3) Inserting "AGO" (Like case 1, but set iscompr = 0 into original node)
     *
     *            |N| -> "NIBALE" -> "SCO" -> []
     *     |A| -> |-|
     *            |G| -> (... continue algo ...) |O| -> []
     *
     * 4) Inserting "CIAO"
     *
     *     |A| -> "NNIBALE" -> "SCO" -> []
     *     |-|
     *     |C| -> (... continue algo ...) "IAO" -> []
     *
     * 5) Inserting "ANNI"
     *
     *     "ANNI" -> "BALE" -> "SCO" -> []
     *
     * The final algorithm for insertion covering all the above cases is as
     * follows.
     *
     * ============================= ALGO 1 =============================
     *
     * For the above cases 1 to 4, that is, all cases where we stopped in
     * the middle of a compressed node for a character mismatch, do:
     *
     * Let $SPLITPOS be the zero-based index at which, in the
     * compressed node array of characters, we found the mismatching
     * character. For example if the node contains "ANNIBALE" and we add
     * "ANNIENTARE" the $SPLITPOS is 4, that is, the index at which the
     * mismatching character is found.
     *
     * 1. Save the current compressed node $NEXT pointer (the pointer to the
     *    child element, that is always present in compressed nodes).
     *
     * 2. Create "split node" having as child the non common letter
     *    at the compressed node. The other non common letter (at the key)
     *    will be added later as we continue the normal insertion algorithm
     *    at step "6".
     *
     * 3a. IF $SPLITPOS == 0:
     *     Replace the old node with the split node, by copying the auxiliary
     *     data if any. Fix parent's reference. Free old node eventually
     *     (we still need its data for the next steps of the algorithm).
     *
     * 3b. IF $SPLITPOS != 0:
     *     Trim the compressed node (reallocating it as well) in order to
     *     contain $splitpos characters. Change child pointer in order to link
     *     to the split node. If new compressed node len is just 1, set
     *     iscompr to 0 (layout is the same). Fix parent's reference.
     *
     * 4a. IF the postfix len (the length of the remaining string of the
     *     original compressed node after the split character) is non zero,
     *     create a "postfix node". If the postfix node has just one character
     *     set iscompr to 0, otherwise iscompr to 1. Set the postfix node
     *     child pointer to $NEXT.
     *
     * 4b. IF the postfix len is zero, just use $NEXT as postfix pointer.
     *
     * 5. Set child[0] of split node to postfix node.
     *
     * 6. Set the split node as the current node, set current index at child[1]
     *    and continue insertion algorithm as usually.
     *
     * ============================= ALGO 2 =============================
     *
     * For case 5, that is, if we stopped in the middle of a compressed
     * node but no mismatch was found, do:
     *
     * Let $SPLITPOS be the zero-based index at which, in the
     * compressed node array of characters, we stopped iterating because
     * there were no more keys character to match. So in the example of
     * the node "ANNIBALE", adding the string "ANNI", the $SPLITPOS is 4.
     *
     * 1. Save the current compressed node $NEXT pointer (the pointer to the
     *    child element, that is always present in compressed nodes).
     *
     * 2. Create a "postfix node" containing all the characters from $SPLITPOS
     *    to the end. Use $NEXT as the postfix node child pointer.
     *    If the postfix node length is 1, set iscompr to 0.
     *    Set the node as a key with the associated value of the new
     *    inserted key.
     *
     * 3. Trim the current node to contain the first $SPLITPOS characters.
     *    As usually if the new node length is just 1, set iscompr to 0.
     *    Take the iskey / associated value as it was in the original node.
     *    Fix the parent's reference.
     *
     * 4. Set the postfix node as the only child pointer of the trimmed
     *    node created at step 1.
     */

    /* ------------------------- ALGORITHM 1 --------------------------- */
    if (h->is_compr && i != len) {
        // debugf("ALGO 1: Stopped at compressed node %.*s (%p)\n",
        //     h->size, h->data, (void*)h);
        // debugf("Still to insert: %.*s\n", (int)(len-i), s+i);
        // debugf("Splitting at %d: '%c'\n", j, ((char*)h->data)[j]);
        // debugf("Other (key) letter is '%c'\n", s[i]);

        /* 1: Save next pointer. */
        rax_node_t **childfield = rax_node_last_child_ptr(h);
        rax_node_t *next;
        memcpy(&next,childfield,sizeof(next));
        // debugf("Next is %p\n", (void*)next);
        // debugf("iskey %d\n", h->iskey);
        if (h->is_key) {
            // debugf("key value is %p\n", rax_get_data(h));
        }

        /* Set the length of the additional nodes we will need. */
        size_t trimmedlen = j;
        size_t postfixlen = h->size - j - 1;
        int split_node_is_key = !trimmedlen && h->is_key && !h->is_null;
        size_t nodesize;

        /* 2: Create the split node. Also allocate the other nodes we'll need
         *    ASAP, so that it will be simpler to handle OOM. */
        rax_node_t *splitnode = rax_new_node(1, split_node_is_key);
        rax_node_t *trimmed = NULL;
        rax_node_t *postfix = NULL;

        if (trimmedlen) {
            nodesize = sizeof(rax_node_t)+trimmedlen+rax_padding(trimmedlen)+
                       sizeof(rax_node_t*);
            if (h->is_key && !h->is_null) nodesize += sizeof(void*);
            trimmed = zmalloc(nodesize);
        }

        if (postfixlen) {
            nodesize = sizeof(rax_node_t)+postfixlen+rax_padding(postfixlen)+
                       sizeof(rax_node_t*);
            postfix = zmalloc(nodesize);
        }

        /* OOM? Abort now that the tree is untouched. */
        if (splitnode == NULL ||
            (trimmedlen && trimmed == NULL) ||
            (postfixlen && postfix == NULL))
        {
            zfree(splitnode);
            zfree(trimmed);
            zfree(postfix);
            errno = ENOMEM;
            return 0;
        }
        splitnode->data[0] = h->data[j];

        if (j == 0) {
            /* 3a: Replace the old node with the split node. */
            if (h->is_key) {
                void *ndata = rax_get_data(h);
                rax_set_data(splitnode,ndata);
            }
            memcpy(parentlink,&splitnode,sizeof(splitnode));
        } else {
            /* 3b: Trim the compressed node. */
            trimmed->size = j;
            memcpy(trimmed->data,h->data,j);
            trimmed->is_compr = j > 1 ? 1 : 0;
            trimmed->is_key = h->is_key;
            trimmed->is_null = h->is_null;
            if (h->is_key && !h->is_null) {
                void *ndata = rax_get_data(h);
                rax_set_data(trimmed,ndata);
            }
            rax_node_t **cp = rax_node_last_child_ptr(trimmed);
            memcpy(cp,&splitnode,sizeof(splitnode));
            memcpy(parentlink,&trimmed,sizeof(trimmed));
            parentlink = cp; /* Set parentlink to splitnode parent. */
            rax->num_nodes++;
        }

        /* 4: Create the postfix node: what remains of the original
         * compressed node after the split. */
        if (postfixlen) {
            /* 4a: create a postfix node. */
            postfix->is_key = 0;
            postfix->is_null = 0;
            postfix->size = postfixlen;
            postfix->is_compr = postfixlen > 1;
            memcpy(postfix->data,h->data+j+1,postfixlen);
            rax_node_t **cp = rax_node_last_child_ptr(postfix);
            memcpy(cp,&next,sizeof(next));
            rax->num_nodes++;
        } else {
            /* 4b: just use next as postfix node. */
            postfix = next;
        }

        /* 5: Set splitnode first child as the postfix node. */
        rax_node_t **splitchild = rax_node_last_child_ptr(splitnode);
        memcpy(splitchild,&postfix,sizeof(postfix));

        /* 6. Continue insertion: this will cause the splitnode to
         * get a new child (the non common character at the currently
         * inserted key). */
        zfree(h);
        h = splitnode;
    } else if (h->is_compr && i == len) {
    /* ------------------------- ALGORITHM 2 --------------------------- */
        // debugf("ALGO 2: Stopped at compressed node %.*s (%p) j = %d\n",
        //     h->size, h->data, (void*)h, j);

        /* Allocate postfix & trimmed nodes ASAP to fail for OOM gracefully. */
        size_t postfixlen = h->size - j;
        size_t nodesize = sizeof(rax_node_t)+postfixlen+rax_padding(postfixlen)+
                          sizeof(rax_node_t*);
        if (data != NULL) nodesize += sizeof(void*);
        rax_node_t *postfix = zmalloc(nodesize);

        nodesize = sizeof(rax_node_t)+j+rax_padding(j)+sizeof(rax_node_t*);
        if (h->is_key && !h->is_null) nodesize += sizeof(void*);
        rax_node_t *trimmed = zmalloc(nodesize);

        if (postfix == NULL || trimmed == NULL) {
            zfree(postfix);
            zfree(trimmed);
            errno = ENOMEM;
            return 0;
        }

        /* 1: Save next pointer. */
        rax_node_t **childfield = rax_node_last_child_ptr(h);
        rax_node_t *next;
        memcpy(&next,childfield,sizeof(next));

        /* 2: Create the postfix node. */
        postfix->size = postfixlen;
        postfix->is_compr = postfixlen > 1;
        postfix->is_key = 1;
        postfix->is_null = 0;
        memcpy(postfix->data,h->data+j,postfixlen);
        rax_set_data(postfix,data);
        rax_node_t **cp = rax_node_last_child_ptr(postfix);
        memcpy(cp,&next,sizeof(next));
        rax->num_nodes++;

        /* 3: Trim the compressed node. */
        trimmed->size = j;
        trimmed->is_compr = j > 1;
        trimmed->is_key = 0;
        trimmed->is_null = 0;
        memcpy(trimmed->data,h->data,j);
        memcpy(parentlink,&trimmed,sizeof(trimmed));
        if (h->is_key) {
            void *aux = rax_get_data(h);
            rax_set_data(trimmed,aux);
        }

        /* Fix the trimmed node child pointer to point to
         * the postfix node. */
        cp = rax_node_last_child_ptr(trimmed);
        memcpy(cp,&postfix,sizeof(postfix));

        /* Finish! We don't need to continue with the insertion
         * algorithm for ALGO 2. The key is already inserted. */
        rax->num_ele++;
        zfree(h);
        return 1; /* Key inserted. */
    }

    /* We walked the radix tree as far as we could, but still there are left
     * chars in our string. We need to insert the missing nodes. */
    while(i < len) {
        rax_node_t *child;

        /* If this node is going to have a single child, and there
         * are other characters, so that that would result in a chain
         * of single-childed nodes, turn it into a compressed node. */
        if (h->size == 0 && len-i > 1) {
            // debugf("Inserting compressed node\n");
            size_t comprsize = len-i;
            if (comprsize > RAX_NODE_MAX_SIZE)
                comprsize = RAX_NODE_MAX_SIZE;
            rax_node_t *newh = rax_compress_node(h,s+i,comprsize,&child);
            if (newh == NULL) goto oom;
            h = newh;
            memcpy(parentlink,&h,sizeof(h));
            parentlink = rax_node_last_child_ptr(h);
            i += comprsize;
        } else {
            // debugf("Inserting normal node\n");
            rax_node_t **new_parentlink;
            rax_node_t *newh = rax_add_child(h,s[i],&child,&new_parentlink);
            if (newh == NULL) goto oom;
            h = newh;
            memcpy(parentlink,&h,sizeof(h));
            parentlink = new_parentlink;
            i++;
        }
        rax->num_nodes++;
        h = child;
    }
    rax_node_t *newh = rax_realloc_for_data(h,data);
    if (newh == NULL) goto oom;
    h = newh;
    if (!h->is_key) rax->num_ele++;
    rax_set_data(h,data);
    memcpy(parentlink,&h,sizeof(h));
    return 1; /* Element inserted. */

oom:
    /* This code path handles out of memory after part of the sub-tree was
     * already modified. Set the node as a key, and then remove it. However we
     * do that only if the node is a terminal node, otherwise if the OOM
     * happened reallocating a node in the middle, we don't need to free
     * anything. */
    if (h->size == 0) {
        h->is_null = 1;
        h->is_key = 1;
        rax->num_ele++; /* Compensate the next remove. */
        latte_assert_with_info(rax_remove(rax,s,i,NULL) != 0, "rax_remove failed");
    }
    errno = ENOMEM;
    return 0;
}

/* Overwriting insert. Just a wrapper for raxGenericInsert() that will
 * update the element if there is already one for the same key. */
int rax_insert(rax_t *rax, unsigned char *s, size_t len, void *data, void **old) {
    return rax_generic_insert(rax,s,len,data,old,1);
}

/* Initialize the stack. */
static inline void rax_stack_init(rax_stack_t *ts) {
    ts->stack = ts->static_items;
    ts->items = 0;
    ts->max_items = RAX_STACK_STATIC_SIZE;
    ts->oom = 0;
}

/* Pop an item from the stack, the function returns NULL if there are no
 * items to pop. */
static inline void *rax_stack_pop(rax_stack_t *ts) {
    if (ts->items == 0) return NULL;
    ts->items--;
    return ts->stack[ts->items];
}

/* Return the stack item at the top of the stack without actually consuming
 * it. */
static inline void *rax_stack_peek(rax_stack_t *ts) {
    if (ts->items == 0) return NULL;
    return ts->stack[ts->items-1];
}

/* Free the stack in case we used heap allocation. */
static inline void rax_stack_free(rax_stack_t *ts) {
    if (ts->stack != ts->static_items) zfree(ts->stack);
}


/* Low level child removal from node. The new node pointer (after the child
 * removal) is returned. Note that this function does not fix the pointer
 * of the parent node in its parent, so this task is up to the caller.
 * The function never fails for out of memory. */
rax_node_t *rax_remove_child(rax_node_t *parent, rax_node_t *child) {
    // debugnode("raxRemoveChild before", parent);
    /* If parent is a compressed node (having a single child, as for definition
     * of the data structure), the removal of the child consists into turning
     * it into a normal node without children. */
    if (parent->is_compr) {
        void *data = NULL;
        if (parent->is_key) data = rax_get_data(parent);
        parent->is_null = 0;
        parent->is_compr = 0;
        parent->size = 0;
        if (parent->is_key) rax_set_data(parent,data);
        // debugnode("raxRemoveChild after", parent);
        return parent;
    }

    /* Otherwise we need to scan for the child pointer and memmove()
     * accordingly.
     *
     * 1. To start we seek the first element in both the children
     *    pointers and edge bytes in the node. */
    rax_node_t **cp = rax_node_first_child_ptr(parent);
    rax_node_t **c = cp;
    unsigned char *e = parent->data;

    /* 2. Search the child pointer to remove inside the array of children
     *    pointers. */
    while(1) {
        rax_node_t *aux;
        memcpy(&aux,c,sizeof(aux));
        if (aux == child) break;
        c++;
        e++;
    }

    /* 3. Remove the edge and the pointer by memmoving the remaining children
     *    pointer and edge bytes one position before. */
    int taillen = parent->size - (e - parent->data) - 1;
    // debugf("raxRemoveChild tail len: %d\n", taillen);
    memmove(e,e+1,taillen);

    /* Compute the shift, that is the amount of bytes we should move our
     * child pointers to the left, since the removal of one edge character
     * and the corresponding padding change, may change the layout.
     * We just check if in the old version of the node there was at the
     * end just a single byte and all padding: in that case removing one char
     * will remove a whole sizeof(void*) word. */
    size_t shift = ((parent->size+4) % sizeof(void*)) == 1 ? sizeof(void*) : 0;

    /* Move the children pointers before the deletion point. */
    if (shift)
        memmove(((char*)cp)-shift,cp,(parent->size-taillen-1)*sizeof(rax_node_t**));

    /* Move the remaining "tail" pointers at the right position as well. */
    size_t valuelen = (parent->is_key && !parent->is_null) ? sizeof(void*) : 0;
    memmove(((char*)c)-shift,c+1,taillen*sizeof(rax_node_t**)+valuelen);

    /* 4. Update size. */
    parent->size--;

    /* realloc the node according to the theoretical memory usage, to free
     * data if we are over-allocating right now. */
    rax_node_t *newnode = zrealloc(parent,rax_node_current_length(parent));
    if (newnode) {
        // debugnode("raxRemoveChild after", newnode);
    }
    /* Note: if rax_realloc() fails we just return the old address, which
     * is valid. */
    return newnode ? newnode : parent;
}

/* Return the memory address where the 'parent' node stores the specified
 * 'child' pointer, so that the caller can update the pointer with another
 * one if needed. The function assumes it will find a match, otherwise the
 * operation is an undefined behavior (it will continue scanning the
 * memory without any bound checking). */
rax_node_t **rax_find_parent_link(rax_node_t *parent, rax_node_t *child) {
    rax_node_t **cp = rax_node_first_child_ptr(parent);
    rax_node_t *c;
    while(1) {
        memcpy(&c,cp,sizeof(c));
        if (c == child) break;
        cp++;
    }
    return cp;
}



/* Remove the specified item. Returns 1 if the item was found and
 * deleted, 0 otherwise. */
int rax_remove(rax_t *rax, unsigned char *s, size_t len, void **old) {
    rax_node_t *h;
    rax_stack_t ts;

    // debugf("### Delete: %.*s\n", (int)len, s);
    rax_stack_init(&ts);
    int splitpos = 0;
    size_t i = rax_low_walk(rax,s,len,&h,NULL,&splitpos,&ts);
    if (i != len || (h->is_compr && splitpos != 0) || !h->is_key) {
        rax_stack_free(&ts);
        return 0;
    }
    if (old) *old = rax_get_data(h);
    h->is_key = 0;
    rax->num_ele--;

    /* If this node has no children, the deletion needs to reclaim the
     * no longer used nodes. This is an iterative process that needs to
     * walk the three upward, deleting all the nodes with just one child
     * that are not keys, until the head of the rax is reached or the first
     * node with more than one child is found. */

    int trycompress = 0; /* Will be set to 1 if we should try to optimize the
                            tree resulting from the deletion. */

    if (h->size == 0) {
        // debugf("Key deleted in node without children. Cleanup needed.\n");
        rax_node_t *child = NULL;
        while(h != rax->head) {
            child = h;
            // debugf("Freeing child %p [%.*s] key:%d\n", (void*)child,
            //     (int)child->size, (char*)child->data, child->is_key);
            zfree(child);
            rax->num_nodes--;
            h = rax_stack_pop(&ts);
             /* If this node has more then one child, or actually holds
              * a key, stop here. */
            if (h->is_key || (!h->is_compr && h->size != 1)) break;
        }
        if (child) {
            // debugf("Unlinking child %p from parent %p\n",
            //     (void*)child, (void*)h);
            rax_node_t *new = rax_remove_child(h,child);
            if (new != h) {
                rax_node_t *parent = rax_stack_peek(&ts);
                rax_node_t **parentlink;
                if (parent == NULL) {
                    parentlink = &rax->head;
                } else {
                    parentlink = rax_find_parent_link(parent,h);
                }
                memcpy(parentlink,&new,sizeof(new));
            }

            /* If after the removal the node has just a single child
             * and is not a key, we need to try to compress it. */
            if (new->size == 1 && new->is_key == 0) {
                trycompress = 1;
                h = new;
            }
        }
    } else if (h->size == 1) {
        /* If the node had just one child, after the removal of the key
         * further compression with adjacent nodes is potentially possible. */
        trycompress = 1;
    }

    /* Don't try node compression if our nodes pointers stack is not
     * complete because of OOM while executing raxLowWalk() */
    if (trycompress && ts.oom) trycompress = 0;

    /* Recompression: if trycompress is true, 'h' points to a radix tree node
     * that changed in a way that could allow to compress nodes in this
     * sub-branch. Compressed nodes represent chains of nodes that are not
     * keys and have a single child, so there are two deletion events that
     * may alter the tree so that further compression is needed:
     *
     * 1) A node with a single child was a key and now no longer is a key.
     * 2) A node with two children now has just one child.
     *
     * We try to navigate upward till there are other nodes that can be
     * compressed, when we reach the upper node which is not a key and has
     * a single child, we scan the chain of children to collect the
     * compressible part of the tree, and replace the current node with the
     * new one, fixing the child pointer to reference the first non
     * compressible node.
     *
     * Example of case "1". A tree stores the keys "FOO" = 1 and
     * "FOOBAR" = 2:
     *
     *
     * "FOO" -> "BAR" -> [] (2)
     *           (1)
     *
     * After the removal of "FOO" the tree can be compressed as:
     *
     * "FOOBAR" -> [] (2)
     *
     *
     * Example of case "2". A tree stores the keys "FOOBAR" = 1 and
     * "FOOTER" = 2:
     *
     *          |B| -> "AR" -> [] (1)
     * "FOO" -> |-|
     *          |T| -> "ER" -> [] (2)
     *
     * After the removal of "FOOTER" the resulting tree is:
     *
     * "FOO" -> |B| -> "AR" -> [] (1)
     *
     * That can be compressed into:
     *
     * "FOOBAR" -> [] (1)
     */
    if (trycompress) {
        // debugf("After removing %.*s:\n", (int)len, s);
        // debugnode("Compression may be needed",h);
        // debugf("Seek start node\n");

        /* Try to reach the upper node that is compressible.
         * At the end of the loop 'h' will point to the first node we
         * can try to compress and 'parent' to its parent. */
        rax_node_t *parent;
        while(1) {
            parent = rax_stack_pop(&ts);
            if (!parent || parent->is_key ||
                (!parent->is_compr && parent->size != 1)) break;
            h = parent;
            // debugnode("Going up to",h);
        }
        rax_node_t *start = h; /* Compression starting node. */

        /* Scan chain of nodes we can compress. */
        size_t comprsize = h->size;
        int nodes = 1;
        while(h->size != 0) {
            rax_node_t **cp = rax_node_last_child_ptr(h);
            memcpy(&h,cp,sizeof(h));
            if (h->is_key || (!h->is_compr && h->size != 1)) break;
            /* Stop here if going to the next node would result into
             * a compressed node larger than h->size can hold. */
            if (comprsize + h->size > RAX_NODE_MAX_SIZE) break;
            nodes++;
            comprsize += h->size;
        }
        if (nodes > 1) {
            /* If we can compress, create the new node and populate it. */
            size_t nodesize =
                sizeof(rax_node_t)+comprsize+rax_padding(comprsize)+sizeof(rax_node_t*);
            rax_node_t *new = zmalloc(nodesize);
            /* An out of memory here just means we cannot optimize this
             * node, but the tree is left in a consistent state. */
            if (new == NULL) {
                rax_stack_free(&ts);
                return 1;
            }
            new->is_key = 0;
            new->is_null = 0;
            new->is_compr = 1;
            new->size = comprsize;
            rax->num_nodes++;

            /* Scan again, this time to populate the new node content and
             * to fix the new node child pointer. At the same time we free
             * all the nodes that we'll no longer use. */
            comprsize = 0;
            h = start;
            while(h->size != 0) {
                memcpy(new->data+comprsize,h->data,h->size);
                comprsize += h->size;
                rax_node_t **cp = rax_node_last_child_ptr(h);
                rax_node_t *tofree = h;
                memcpy(&h,cp,sizeof(h));
                zfree(tofree); rax->num_nodes--;
                if (h->is_key || (!h->is_compr && h->size != 1)) break;
            }
            // debugnode("New node",new);

            /* Now 'h' points to the first node that we still need to use,
             * so our new node child pointer will point to it. */
            rax_node_t **cp = rax_node_last_child_ptr(new);
            memcpy(cp,&h,sizeof(h));

            /* Fix parent link. */
            if (parent) {
                rax_node_t **parentlink = rax_find_parent_link(parent,start);
                memcpy(parentlink,&new,sizeof(new));
            } else {
                rax->head = new;
            }

            // debugf("Compressed %d nodes, %d total bytes\n",
            //     nodes, (int)comprsize);
        }
    }
    rax_stack_free(&ts);
    return 1;
}