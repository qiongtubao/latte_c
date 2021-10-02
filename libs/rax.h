#ifndef RAX_H
#define RAX_H

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
/* Representation of a radix tree as implemented in this file, that contains
 * the strings "foo", "foobar" and "footer" after the insertion of each
 * word. When the node represents a key inside the radix tree, we write it
 * between [], otherwise it is written between ().
 *
 * This is the vanilla representation:
 *
 *              (f) ""
 *                \
 *                (o) "f"
 *                  \
 *                  (o) "fo"
 *                    \
 *                  [t   b] "foo"
 *                  /     \
 *         "foot" (e)     (a) "foob"
 *                /         \
 *      "foote" (r)         (r) "fooba"
 *              /             \
 *    "footer" []             [] "foobar"
 *
 * However, this implementation implements a very common optimization where
 * successive nodes having a single child are "compressed" into the node
 * itself as a string of characters, each representing a next-level child,
 * and only the link to the node representing the last character node is
 * provided inside the representation. So the above representation is turned
 * into:
 *
 *                  ["foo"] ""
 *                     |
 *                  [t   b] "foo"
 *                  /     \
 *        "foot" ("er")    ("ar") "foob"
 *                 /          \
 *       "footer" []          [] "foobar"
 *
 * However this optimization makes the implementation a bit more complex.
 * For instance if a key "first" is added in the above radix tree, a
 * "node splitting" operation is needed, since the "foo" prefix is no longer
 * composed of nodes having a single child one after the other. This is the
 * above tree and the resulting node splitting after this event happens:
 *
 *
 *                    (f) ""
 *                    /
 *                 (i o) "f"
 *                 /   \
 *    "firs"  ("rst")  (o) "fo"
 *              /        \
 *    "first" []       [t   b] "foo"
 *                     /     \
 *           "foot" ("er")    ("ar") "foob"
 *                    /          \
 *          "footer" []          [] "foobar"
 *
 * Similarly after deletion, if a new chain of nodes having a single child
 * is created (the chain must also not include nodes that represent keys),
 * it must be compressed back into a single node.
 *
 */

// 基数树节点定义
typedef struct raxNode {
    uint32_t iskey:1; // 该节点是否构成键
    uint32_t isnull:1; // 键对应的值是否为null，为null则不存储
    uint32_t iscompr:1; // 该节点是否为压缩节点
    uint32_t size:29; // 子节点数量，或者压缩后字符串长度
    /* 
     * 如果节点未被压缩，则存储一个长度为n的字符串。
     * 字符串后面是n个指针，分别指向对应位置的字符的子节点。
     * 末尾是一个该节点键对应的值（如果存在的话）。
     * 节点样例如下：
     * [header iscompr=0...][abc][a-ptr][b-ptr][c-ptr](value-ptr?)
     * --------------------------------------------
     * 如果节点被压缩，则存储一个长度为n的字符串。
     * 字符串后面是1个指针，指向字符串尾字符对应的子节点。
     * 末尾同样是一个节点键对应的值（如果存在的话）。
     * 节点样例如下：
     * [header iscompr=1...][xyz][z-ptr](value-ptr?)
     */
    unsigned char data[];
} raxNode;
// 基数树定义
typedef struct rax {
    raxNode *head; // 树根节点
    uint64_t numele; // 元素数
    uint64_t numnodes; // 节点数
} rax;

// rax栈，在移除节点是会用到，用于记录遍历路径节点信息
#define RAX_STACK_STATIC_ITEMS 32
typedef struct raxStack {
    void **stack; // 节点访问顺序指针
    size_t items, maxitems;
    void *static_items[RAX_STACK_STATIC_ITEMS];
    int oom; 
} raxStack;
/* Optional callback used for iterators and be notified on each rax node,
 * including nodes not representing keys. If the callback returns true
 * the callback changed the node pointer in the iterator structure, and the
 * iterator implementation will have to replace the pointer in the radix tree
 * internals. This allows the callback to reallocate the node to perform
 * very special operations, normally not needed by normal applications.
 *
 * This callback is used to perform very low level analysis of the radix tree
 * structure, scanning each possible node (but the root node), or in order to
 * reallocate the nodes to reduce the allocation fragmentation (this is the
 * Redis application for this callback).
 *
 * This is currently only supported in forward iterations (raxNext) */
typedef int (*raxNodeCallback)(raxNode **noderef);
/* Radix tree iterator state is encapsulated into this data structure. */
#define RAX_ITER_STATIC_LEN 128
#define RAX_ITER_JUST_SEEKED (1<<0) /* Iterator was just seeked. Return current
                                       element for the first iteration and
                                       clear the flag. */
#define RAX_ITER_EOF (1<<1)    /* End of iteration reached. */
#define RAX_ITER_SAFE (1<<2)   /* Safe iterator, allows operations while
                                  iterating. But it is slower. */

typedef struct raxIterator {
    int flags;
    rax *rt;                /* Radix tree we are iterating. */
    unsigned char *key;     /* The current string. */
    void *data;             /* Data associated to this key. */
    size_t key_len;         /* Current key length. */
    size_t key_max;         /* Max key len the current key buffer can hold. */
    unsigned char key_static_string[RAX_ITER_STATIC_LEN];
    raxNode *node;          /* Current node. Only for unsafe iteration. */
    raxStack stack;         /* Stack used for unsafe iteration. */
    raxNodeCallback node_cb; /* Optional node callback. Normally set to NULL. */
} raxIterator;
/* A special pointer returned for not found items. */
extern void *raxNotFound;
/* Exported API. */
rax *raxNew(void);
int raxInsert(rax *rax, unsigned char *s, size_t len, void *data, void **old);
int raxTryInsert(rax *rax, unsigned char *s, size_t len, void *data, void **old);
int raxRemove(rax *rax, unsigned char *s, size_t len, void **old);
void *raxFind(rax *rax, unsigned char *s, size_t len);
void raxFree(rax *rax);
void raxFreeWithCallback(rax *rax, void (*free_callback)(void*));
void raxStart(raxIterator *it, rax *rt);
int raxSeek(raxIterator *it, const char *op, unsigned char *ele, size_t len);
int raxNext(raxIterator *it);
int raxPrev(raxIterator *it);
int raxRandomWalk(raxIterator *it, size_t steps);
int raxCompare(raxIterator *iter, const char *op, unsigned char *key, size_t key_len);
void raxStop(raxIterator *it);
int raxEOF(raxIterator *it);
void raxShow(rax *rax);
uint64_t raxSize(rax *rax);
unsigned long raxTouch(raxNode *n);
void raxSetDebugMsg(int onoff);

/* Internal API. May be used by the node callback in order to access rax nodes
 * in a low level way, so this function is exported as well. */
void raxSetData(raxNode *n, void *data);
#endif
