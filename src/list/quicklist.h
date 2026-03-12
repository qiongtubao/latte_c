/**
 * @file quicklist.h
 * @brief 快速链表（quicklist）接口
 *        quicklist 是一种双向链表 + listpack 的混合结构：
 *        每个链表节点（quick_list_node_t）内部存储一个 listpack（或 LZF 压缩块），
 *        兼顾了链表的灵活性和连续内存的缓存友好性，是 Redis list 类型的底层实现。
 *        支持节点级 LZF 压缩以节省内存，压缩范围由 compress 参数控制。
 */

#ifndef __quick_list_H__
#define __quick_list_H__
#include "zmalloc/zmalloc.h"
#include <stdint.h>

/** @brief quicklist 节点容器类型：PLAIN（单个条目，直接存 char 数组） */
#define quick_list_NODE_CONTAINER_PLAIN 1
/** @brief quicklist 节点容器类型：PACKED（listpack，存储多个条目） */
#define quick_list_NODE_CONTAINER_PACKED 2

/** @brief push/pop 位置：头部 */
#define quick_list_HEAD 0
/** @brief push/pop 位置：尾部 */
#define quick_list_TAIL -1

/** @brief 判断节点是否为 PLAIN 容器类型 */
#define QL_NODE_IS_PLAIN(node) ((node)->container == quick_list_NODE_CONTAINER_PLAIN)


/** @brief quicklist 节点编码：未压缩（原始 listpack） */
#define quick_list_NODE_ENCODING_RAW 1
/** @brief quicklist 节点编码：LZF 压缩 */
#define quick_list_NODE_ENCODING_LZF 2

/* 根据平台字长确定位域宽度 */
#if UINTPTR_MAX == 0xffffffff
/* 32位平台 */
#   define QL_FILL_BITS 14   /**< 填充因子位域宽度（32位） */
#   define QL_COMP_BITS 14   /**< 压缩参数位域宽度（32位） */
#   define QL_BM_BITS 4      /**< 书签计数位域宽度（32位） */
#elif UINTPTR_MAX == 0xffffffffffffffff
/* 64位平台 */
#   define QL_FILL_BITS 16   /**< 填充因子位域宽度（64位） */
#   define QL_COMP_BITS 16   /**< 压缩参数位域宽度（64位） */
#   define QL_BM_BITS 4      /**< 书签计数位域宽度（64位，限制用户可用数量以避免性能下降） */
#else
#   error unknown arch bits count
#endif

/**
 * @brief quicklist 节点结构体（32 字节）
 *        描述 quicklist 中单个节点，使用位域使结构体保持 32 字节。
 *        count 最大 65536（listpack 最大字节数为 65k，实际 count < 32k）。
 */
typedef struct quick_list_node_t {
    struct quick_list_node_t *prev;  /**< 前驱节点指针 */
    struct quick_list_node_t *next;  /**< 后继节点指针 */
    unsigned char *entry;            /**< 节点数据：listpack 或 LZF 压缩块 */
    size_t sz;                       /**< entry 的字节大小 */
    unsigned int count : 16;         /**< listpack 中的条目数量（最大 65536） */
    unsigned int encoding : 2;       /**< 节点编码：RAW==1 或 LZF==2 */
    unsigned int container : 2;      /**< 容器类型：PLAIN==1 或 PACKED==2 */
    unsigned int recompress : 1;     /**< 节点是否被临时解压（使用后需重新压缩） */
    unsigned int attempted_compress : 1; /**< 节点已尝试压缩但太小，测试时用于验证 */
    unsigned int dont_compress : 1;  /**< 阻止压缩：该节点将被立即使用 */
    unsigned int extra : 9;          /**< 预留位，供未来功能使用 */
} quick_list_node_t;

/**
 * @brief quicklist 书签结构体
 *        书签是可选功能，通过 realloc quicklist 结构体尾部追加，
 *        用于对超大列表分段迭代时保存游标。
 *        使用时有内存开销，但删除后保留少量开销（避免频繁 realloc 导致震荡）。
 *        书签数量应尽量少，因为删除节点时需要搜索所有书签进行更新。
 */
typedef struct quick_list_bookmark_t {
    quick_list_node_t *node; /**< 书签指向的节点 */
    char *name;              /**< 书签名称字符串 */
} quick_list_bookmark_t;

/**
 * @brief quicklist 结构体（64位系统上为 40 字节）
 *        描述整个快速链表，包含头尾指针、元素总数、节点数量及压缩/填充策略。
 */
typedef struct quick_list_t {
    quick_list_node_t *head;           /**< 头节点指针 */
    quick_list_node_t *tail;           /**< 尾节点指针 */
    unsigned long count;               /**< 所有节点中的条目总数 */
    unsigned long len;                 /**< quicklist 节点（链表节点）个数 */
    signed int fill: QL_FILL_BITS;     /**< 填充因子：每个节点的最大条目数或最大字节数（用户配置或默认值） */
    unsigned int compress: QL_COMP_BITS; /**< 压缩参数：0 表示禁止压缩；>0 表示两端各保留未压缩节点的数量 */
    unsigned int bookmark_count: QL_BM_BITS; /**< 书签数量 */
    quick_list_bookmark_t bookmarks[]; /**< 书签数组（柔性数组，通过 realloc 追加） */
} quick_list_t;

/**
 * @brief LZF 压缩节点结构体
 *        存储节点的 LZF 压缩数据，sz 为压缩后字节数，compressed 为压缩内容。
 */
typedef struct quick_list_LZF_t {
    size_t sz;            /**< LZF 压缩数据大小（字节） */
    char compressed[];    /**< LZF 压缩内容（柔性数组） */
} quick_list_LZF_t;


/* ---- quicklist API ---- */

/**
 * @brief 创建指定填充因子和压缩参数的 quicklist
 * @param fill     填充因子（正数=最大条目数，负数=最大字节限制）
 * @param compress 压缩参数（0=不压缩，N=两端各保留 N 个未压缩节点）
 * @return 新建的 quick_list_t 指针；内存分配失败返回 NULL
 */
struct quick_list_t* quick_list_new(int fill, int compress);

/**
 * @brief 创建使用默认参数的 quicklist
 * @return 新建的 quick_list_t 指针；内存分配失败返回 NULL
 */
struct quick_list_t* quick_list_create(void);

/**
 * @brief 在 quicklist 头部或尾部推入元素
 * @param quick_list 目标 quicklist 指针
 * @param value      要插入的数据指针
 * @param sz         数据大小（字节）
 * @param where      插入位置：quick_list_HEAD 或 quick_list_TAIL
 */
void quick_list_push(quick_list_t *quick_list, void *value, const size_t sz,
                   int where);

/**
 * @brief 在 quicklist 头部推入元素
 * @param quick_list 目标 quicklist 指针
 * @param value      要插入的数据指针
 * @param sz         数据大小（字节）
 * @return 成功返回 1；失败返回 0
 */
int quick_list_push_head(quick_list_t *quick_list, void *value, size_t sz);

/**
 * @brief 打印 quicklist 内容（调试用）
 * @param ql   quicklist 指针（unsigned char* 形式）
 * @param full 非零时打印每个节点的详细内容
 */
void quick_list_repr(unsigned char* ql, int full);

/**
 * @brief 释放 quicklist 及其所有节点
 * @param quick_list 要释放的 quick_list_t 指针（NULL 时安全跳过）
 */
void quick_list_release(quick_list_t* quick_list);

/**
 * @brief 创建新的 quicklist 节点
 * @return 新建的 quick_list_node_t 指针；内存分配失败返回 NULL
 */
struct quick_list_node_t *quick_list_new_node(void);

#endif /* __quick_list_H__ */
