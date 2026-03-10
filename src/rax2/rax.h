/* Rax -- A radix tree implementation.
 *
 * Copyright (c) 2017-2018, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RAX_H
#define RAX_H

#include <stdint.h>
#include <stdlib.h>


/**
 * @brief 
 *  rax是Redis使用的一种紧凑的有序字典数据结构，
    用于实现跳跃表和压缩列表。在Redis中，
    rax用于高效地存储和检索有序数据。
 */

/* 
    在此文件中实现的基数树的表示，
    包含在每个单词插入后的字符串 
    "foo"、"foobar" 和 "footer"。
    当节点表示基数树中的键时，
    我们将其写在 [] 之间，否则写在 () 之间。
*/
/* Representation of a radix tree as implemented in this file, that contains
 * the strings "foo", "foobar" and "footer" after the insertion of each
 * word. When the node represents a key inside the radix tree, we write it
 * between [], otherwise it is written between ().
 *
 * This is the vanilla representation:
 * 
 * 此文件中实现的基数树的表示形式，其中包含
 * 插入后的字符串“foo”，“foobar”和“footer”
 * 词。当节点表示基数树中的一个键时，我们写它
 * 在 [] 之间，否则写在 （） 之间。
 *
 * 这是原版表示：
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
 * 但是，此实现实现了一个非常常见的优化，其中
 * 具有单个子节点的连续节点被“压缩”到节点中
 * 本身为一串字符，每个字符代表下一级子级，
 * 并且只有指向代表最后一个字符节点的节点的链接是
 * 在表示内提供。所以上面的表示被转过来了
 * 到：
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
 * 但是，这种优化使实现更加复杂。
 * 例如，如果在上面的基数树中添加了一个键“first”，则
 * 需要“节点拆分”操作，因为“foo”前缀不再是
 * 由一个接一个具有单个子节点的节点组成。这是
 * 上面的树和在此事件发生后产生的节点分裂：
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
 * 同样，删除后，如果具有单个子节点的新节点链
 * 被创建（链也不能包含表示键的节点），
 * 必须将其压缩回单个节点。
 * 
 */

#define RAX_NODE_MAX_SIZE ((1<<29)-1)
/**
 * @brief 
 * 
    - `iskey`：一个标志，指示节点是否是一个键的终止节点。
        如果为1，表示节点是一个键的终止节点；
        如果为0，表示节点不是键的终止节点。
    - `iscompr`：一个标志，指示节点是否是一个压缩节点。
        如果为1，表示节点是一个压缩节点；
        如果为0，表示节点不是压缩节点。
    - `size`：节点的大小，即节点表示的键的长度。
    - `data`：一个指针，指向节点存储的键的数据。
    - `children`：一个指针数组，用于存储节点的子节点。
        对于非压缩节点，子节点按照字典序排列；
        对于压缩节点，子节点按照它们在压缩节点中的顺序排列。
    - `parent`：一个指针，指向节点的父节点。
    - `iskey`和`iscompr`属性的组合形成了节点的类型，可以是非压缩节点、压缩节点或叶子节点。
 * 
 */
typedef struct raxNode {
    uint32_t iskey:1;     /* Does this node contain a key? 此节点是否包含键 */
    uint32_t isnull:1;    /* Associated value is NULL (don't store it). 关联的值为 NULL（不存储它）。*/
    uint32_t iscompr:1;   /* Node is compressed. 节点被压缩。 */
    uint32_t size:29;     /* Number of children, or compressed string len. 子项数，或压缩字符串 len。*/
    /* 数据布局如下：
     *
     * 如果节点没有被压缩，我们有“大小”字节，每个子字节一个
     * 字符和“size”raxNode 指针指向每个子节点。
     *注意字符如何不是存储在子项中，而是存储在
     * 父母的边缘：
     *
     * [header iscompr=0][abc][a-ptr][b-ptr][c-ptr]（value-ptr？）
     *
     * 如果节点被压缩（IScompr 位为 1），则节点有 1 个子节点。
     * 在这种情况下，字符串的“大小”字节立即存储在
     * 数据部分的开头，表示连续的序列
     * 节点一个接一个地链接，其中只有最后一个
     * 序列实际上表示为一个节点，并指向
     * 当前压缩节点。
     * 
     * 如果节点被压缩（ISCOMPR 位为 1），则节点有 1 个子节点。
     * 在这种情况下，字符串的“大小”字节立即存储在
     * 数据部分的开头，表示连续的序列
     * 节点一个接一个地链接，其中只有最后一个
     * 序列实际上表示为一个节点，并指向
     * 当前压缩节点。
     *
     * [header iscompr=1][xyz][z-ptr]（value-ptr？）
     *
     * 压缩节点和未压缩节点都可以表示一个键
     * 基数树中任何级别的关联数据（不仅仅是终端）
     * 节点）。
     *
     * 如果节点具有关联的键 （iskey=1） 并且不是 NULL
     * （isnull=0），然后在指向
     * 子项，存在一个附加值指针（如您所见
     * 在上面的表示形式中为“值-PTR”字段）。
     */
     
    
    /* Data layout is as follows:
     *
     * If node is not compressed we have 'size' bytes, one for each children
     * character, and 'size' raxNode pointers, point to each child node.
     * Note how the character is not stored in the children but in the
     * edge of the parents:
     *
     * [header iscompr=0][abc][a-ptr][b-ptr][c-ptr](value-ptr?)
     *
     * if node is compressed (iscompr bit is 1) the node has 1 children.
     * In that case the 'size' bytes of the string stored immediately at
     * the start of the data section, represent a sequence of successive
     * nodes linked one after the other, for which only the last one in
     * the sequence is actually represented as a node, and pointed to by
     * the current compressed node.
     *
     * [header iscompr=1][xyz][z-ptr](value-ptr?)
     *
     * Both compressed and not compressed nodes can represent a key
     * with associated data in the radix tree at any level (not just terminal
     * nodes).
     *
     * If the node has an associated key (iskey=1) and is not NULL
     * (isnull=0), then after the raxNode pointers pointing to the
     * children, an additional value pointer is present (as you can see
     * in the representation above as "value-ptr" field).
     */
    unsigned char data[];
} raxNode;
/**
 * @brief 
 *      字典
 */
typedef struct rax {
    raxNode *head;
    uint64_t numele;
    uint64_t numnodes;
} rax;

/* Stack data structure used by raxLowWalk() in order to, optionally, return
 * a list of parent nodes to the caller. The nodes do not have a "parent"
 * field for space concerns, so we use the auxiliary stack when needed. */
#define RAX_STACK_STATIC_ITEMS 32
/**
 * @brief 
 *  是Redis使用的一个简单栈数据结构，
 *  用于在rax树的遍历期间跟踪节点。
 *  raxStack中的元素是raxNode指针，
 *  表示rax树的节点。在rax树的遍历期间，
 *  raxStack用于在遍历过程中跟踪已访问的节点，
 *  并在需要时回溯到之前的节点。
 */
typedef struct raxStack {
    void **stack; /* Points to static_items or an heap allocated array. */
    size_t items, maxitems; /* Number of items contained and total space. */
    /* Up to RAXSTACK_STACK_ITEMS items we avoid to allocate on the heap
     * and use this static array of pointers instead. */
    void *static_items[RAX_STACK_STATIC_ITEMS];
    int oom; /* True if pushing into this stack failed for OOM at some point. */
} raxStack;


/* 用于迭代器的可选回调，并在每个 rax 节点上收到通知，
 * 包括不代表键的节点。如果回调返回 true
 * 回调更改了迭代器结构中的节点指针，并且
 * 迭代器实现必须替换基数树中的指针
 *内部。这允许回调重新分配要执行的节点
 *非常特殊的操作，通常不需要正常应用程序。
 *
 * 此回调用于对基数树进行非常低级的分析
 * 结构，扫描每个可能的节点（但根节点），或为了
 * 重新分配节点以减少分配碎片（这是
 * 此回调的 Redis 应用程序）。
 *
 * 目前仅在前向迭代 （raxNext） 中支持此功能 */
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
int raxFind(rax *rax, unsigned char *s, size_t len, void **value);
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