
#ifndef __quick_list_H__
#define __quick_list_H__
#include "zmalloc/zmalloc.h"
#include <stdint.h>

/* quick_list node container formats */
#define quick_list_NODE_CONTAINER_PLAIN 1
#define quick_list_NODE_CONTAINER_PACKED 2

#define quick_list_HEAD 0
#define quick_list_TAIL -1


#define QL_NODE_IS_PLAIN(node) ((node)->container == quick_list_NODE_CONTAINER_PLAIN)


/* quick_list node encodings */
#define quick_list_NODE_ENCODING_RAW 1
#define quick_list_NODE_ENCODING_LZF 2
#if UINTPTR_MAX == 0xffffffff
/* 32-bit */
#   define QL_FILL_BITS 14
#   define QL_COMP_BITS 14
#   define QL_BM_BITS 4
#elif UINTPTR_MAX == 0xffffffffffffffff
/* 64-bit */
#   define QL_FILL_BITS 16
#   define QL_COMP_BITS 16
#   define QL_BM_BITS 4 /* we can encode more, but we rather limit the user
                           since they cause performance degradation. */
#else
#   error unknown arch bits count
#endif

/* quick_listNode是一个32字节的结构体，用于描述quick_list的listpack。
* 我们使用位域将quick_listNode保持在32字节。
* 计数:16位，最大65536(最大lp字节是65k，所以最大计数实际上< 32k)。
* 编码:2位，RAW=1, LZF=2。
* 容器:2位，PLAIN=1(单个项目作为char数组)，PACKED=2(包含多个项目的listpack)。
* recompress: 1位，bool，如果节点被临时解压以供使用，则为true。
* attempted_compress: 1位，布尔值，用于测试时验证。
* 额外:10位，免费供以后使用;填充32位的剩余部分*/
typedef struct quick_list_node_t {
    struct quick_list_node_t *prev;
    struct quick_list_node_t *next;
    unsigned char *entry;
    size_t sz;             /* entry size in bytes */
    unsigned int count : 16;     /* count of items in listpack */
    unsigned int encoding : 2;   /* RAW==1 or LZF==2 */
    unsigned int container : 2;  /* PLAIN==1 or PACKED==2 */
    unsigned int recompress : 1; /* was this node previous compressed? */
    unsigned int attempted_compress : 1; /* node can't compress; too small */
    unsigned int dont_compress : 1; /* prevent compression of entry that will be used later */
    unsigned int extra : 9; /* more bits to steal for future usage */
} quick_list_node_t;
/*
 * 书签在快速列表结构体的末尾填充realloc。
 * 它们应该只用于非常大的列表，如果数千个节点是
 * 多余的内存使用可以忽略不计，并且确实需要对它们进行迭代
 * 分份。
 * 当不使用时，它们不会增加任何内存开销，但当使用时，然后
 * 删除，保留一些开销(以避免共振)。
 * 使用的书签数量应保持在最低限度，因为它也增加
   删除节点时的开销(搜索要更新的书签)。
*/
typedef struct quick_list_bookmark_t {
    quick_list_node_t *node;
    char *name;
} quick_list_bookmark_t;
/*
   'quick_list'： 是一个40字节的结构体(在64位系统上)，描述一个快速列表。
   'count'： 是总条目数。
   'len'： 是快速列表节点的个数。
   'compress': 如果禁用压缩，则为0，否则是在末尾保留未压缩的quick_listNodes的数量
   'fill' 是用户请求的(或默认的)填充因子。
   'bookmarks' 是realloc this结构体使用的可选功能,使它们在不使用时不会消耗内存。
 */
typedef struct quick_list_t {
    quick_list_node_t *head;
    quick_list_node_t *tail;
    unsigned long count;
    unsigned long len;
    signed int fill: QL_FILL_BITS;
    unsigned int compress: QL_COMP_BITS;
    unsigned int bookmark_count: QL_BM_BITS;
    quick_list_bookmark_t bookmarks[];
} quick_list_t;

typedef struct quick_list_LZF_t {
    size_t sz; /* LZF size in bytes*/
    char compressed[];
} quick_list_LZF_t;



//quick_list methods
struct quick_list_t* quick_list_new(int fill, int compress);
struct quick_list_t* quick_list_create(void);

//
void quick_list_push(quick_list_t *quick_list, void *value, const size_t sz,
                   int where);

int quick_list_push_head(quick_list_t *quick_list, void *value, size_t sz);
void quick_list_repr(unsigned  char* ql, int full);
void quick_list_release(quick_list_t* quick_list);
//quick_listNode methods
struct quick_list_node_t *quick_list_new_node(void);
#endif /* __quick_list_H__ */