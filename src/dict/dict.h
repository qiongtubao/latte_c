/**
 * @file dict.h
 * @brief 哈希字典（Hash Table）接口
 *        基于开放寻址 + 渐进式 rehash 的哈希表实现，参考 Redis 7.2.5 设计。
 *        支持任意类型键值对（void*）、自定义哈希/比较/析构函数、
 *        动态扩缩容（渐进式 rehash 避免单次阻塞）。
 */

#ifndef __LATTE_DICT_H
#define __LATTE_DICT_H

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include "iterator/iterator.h"
#include "cmp/cmp.h"

/** @brief 操作成功返回值 */
#define DICT_OK 0
/** @brief 操作失败返回值 */
#define DICT_ERR 1

/** @brief 抑制"未使用参数"编译警告的辅助宏 */
#define DICT_NOTUSED(V) ((void) V)


/**
 * @brief 哈希表节点结构体
 *        存储单个键值对，支持链式解决哈希冲突，并可附带调用方自定义元数据。
 */
typedef struct dict_entry_t {
    void *key;                  /**< 节点键指针 */
    union {
        void *val;              /**< 通用值指针 */
        uint64_t u64;           /**< uint64_t 整型值（节省内存，避免额外分配） */
        int64_t s64;            /**< int64_t 整型值 */
        double d;               /**< 双精度浮点值 */
    } v;                        /**< 节点值（联合体，按实际类型选用） */
    struct dict_entry_t *next;  /**< 同一桶中下一个节点（链表法解决冲突） */
    void *metadata[];           /**< 调用方自定义元数据（柔性数组，按指针对齐；
                                 *   大小由 dict_func_t.dictEntryMetadataBytes 决定） */
} dict_entry_t;

typedef struct dict_t dict_t;

/**
 * @brief 哈希字典函数表结构体
 *        提供哈希、键值复制/比较/析构等回调，实现泛型键值类型支持。
 */
typedef struct dict_func_t {
    uint64_t (*hashFunction)(const void *key);                        /**< 哈希函数：计算 key 的哈希值 */
    void *(*keyDup)(dict_t *d, const void *key);                      /**< 键复制函数（NULL 表示不复制，直接存指针） */
    void *(*valDup)(dict_t *d, const void *obj);                      /**< 值复制函数（NULL 表示不复制，直接存指针） */
    int (*keyCompare)(dict_t *d, const void *key1, const void *key2); /**< 键比较函数（NULL 表示用指针相等） */
    void (*keyDestructor)(dict_t *d, void *key);                      /**< 键析构函数（NULL 表示不释放） */
    void (*valDestructor)(dict_t *d, void *obj);                      /**< 值析构函数（NULL 表示不释放） */
    int (*expandAllowed)(size_t moreMem, double usedRatio);           /**< 允许扩容判断函数（NULL 表示总是允许） */
    /** @brief 每个节点附带的调用方元数据字节数（NULL 或返回 0 表示无元数据） */
    size_t (*dictEntryMetadataBytes)(dict_t *d);
} dict_func_t;

/**
 * @brief 由哈希表大小指数计算实际容量（2^exp；exp == -1 表示空表）
 * @param exp 哈希表大小指数（-1 表示空）
 */
#define DICTHT_SIZE(exp) ((exp) == -1 ? 0 : (unsigned long)1<<(exp))

/**
 * @brief 由哈希表大小指数计算掩码（用于快速取模：hash & mask）
 * @param exp 哈希表大小指数（-1 表示空）
 */
#define DICTHT_SIZE_MASK(exp) ((exp) == -1 ? 0 : (DICTHT_SIZE(exp))-1)

/** @brief 哈希表最小填充率（10%），低于此时触发缩容 */
#define HASHTABLE_MIN_FILL        10

/**
 * @brief 获取字典总槽位数（ht_table[0] + ht_table[1]，rehash 期间两张表均有效）
 * @param d dict_t 指针
 */
#define dict_slots(d) (DICTHT_SIZE((d)->ht_size_exp[0])+DICTHT_SIZE((d)->ht_size_exp[1]))

/**
 * @brief 哈希字典结构体
 *        可参考 Redis 7.2.5 实现，支持渐进式 rehash（双哈希表）。
 */
struct dict_t {
    dict_func_t *type;              /**< 函数表指针，提供哈希/比较/析构等回调 */

    dict_entry_t **ht_table[2];     /**< 双哈希桶数组：[0] 为主表，[1] 为 rehash 目标表 */
    unsigned long ht_used[2];       /**< 各哈希表当前已使用节点数 */

    long rehashidx;                 /**< 渐进式 rehash 进度（== -1 表示未在 rehash） */

    /* 小变量放末尾，减少结构体填充（padding）开销 */
    int16_t pauserehash;            /**< >0 时暂停 rehash；<0 表示编码错误 */
    signed char ht_size_exp[2];     /**< 两张哈希表的大小指数（容量 = 1<<exp） */

    void* metadata[];               /**< 调用方扩展元数据（如 expire 子键，柔性数组） */
};


/** @brief 哈希表初始大小指数（初始容量 = 1<<2 = 4） */
#define DICT_HT_INITIAL_EXP      2
/** @brief 哈希表初始容量 */
#define DICT_HT_INITIAL_SIZE     (1<<(DICT_HT_INITIAL_EXP))


/**
 * @brief 调用值析构函数释放节点值
 * @param d     dict_t 指针
 * @param entry dict_entry_t 指针
 */
#define dict_free_val(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d), (entry)->v.val)

/**
 * @brief 调用键析构函数释放节点键
 * @param d     dict_t 指针
 * @param entry dict_entry_t 指针
 */
#define dict_free_key(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d), (entry)->key)

/**
 * @brief 设置节点键（若有 keyDup 则复制，否则直接赋指针）
 * @param d      dict_t 指针
 * @param entry  dict_entry_t 指针
 * @param _key_  要设置的键指针
 */
#define dict_set_key(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        (entry)->key = (d)->type->keyDup((d), _key_); \
    else \
        (entry)->key = (_key_); \
} while(0)

/**
 * @brief 比较两个键（若有 keyCompare 回调则调用，否则使用指针相等）
 * @param d    dict_t 指针
 * @param key1 第一个键指针
 * @param key2 第二个键指针
 * @return 相等返回非零；不等返回 0
 */
#define dict_compare_keys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d), key1, key2) : \
        (key1) == (key2))

/**
 * @brief 获取节点元数据指针（指向 metadata 柔性数组首地址）
 * @param entry dict_entry_t 指针
 */
#define dict_get_meta_data(entry) (&(entry)->metadata)

/**
 * @brief 获取字典节点元数据字节数（无 dictEntryMetadataBytes 回调时返回 0）
 * @param d dict_t 指针
 */
#define dict_get_meta_data_size(d) ((d)->type->dictEntryMetadataBytes \
                             ? (d)->type->dictEntryMetadataBytes(d) : 0)

/**
 * @brief 获取字典当前元素总数（两张哈希表之和）
 * @param d dict_t 指针
 */
#define dict_size(d) ((d)->ht_used[0]+(d)->ht_used[1])

/**
 * @brief 判断字典是否正在进行渐进式 rehash
 * @param d dict_t 指针
 * @return rehashidx != -1 时返回真（非零）
 */
#define dict_is_rehashing(d) ((d)->rehashidx != -1)

/**
 * @brief 计算 key 的哈希值（调用 type->hashFunction）
 * @param d   dict_t 指针
 * @param key 键指针
 */
#define dict_hash_key(d, key) (d)->type->hashFunction(key)


/**
 * @brief 设置节点值（若有 valDup 则复制，否则直接赋指针）
 * @param d      dict_t 指针
 * @param entry  dict_entry_t 指针
 * @param _val_  要设置的值指针
 */
#define dict_set_val(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        (entry)->v.val = (d)->type->valDup((d), _val_); \
    else \
        (entry)->v.val = (_val_); \
} while(0)

/** @brief 暂停渐进式 rehash（pauserehash++） */
#define dict_pause_rehashing(d) (d)->pauserehash++
/** @brief 恢复渐进式 rehash（pauserehash--） */
#define dict_resume_rehashing(d) (d)->pauserehash--

/** @brief 获取节点键指针 */
#define dict_get_entry_key(he) ((he)->key)
/** @brief 获取节点值指针 */
#define dict_get_entry_val(he) ((he)->v.val)

/* ---- 字典 API ---- */

/**
 * @brief 创建新字典
 * @param type 函数表指针（不可为 NULL）
 * @return 新建的 dict_t 指针；内存分配失败返回 NULL
 */
dict_t* dict_new(dict_func_t *type);

/**
 * @brief 销毁字典及所有节点（调用析构函数释放键值）
 * @param d dict_t 指针
 */
void dict_delete(dict_t *d);

/**
 * @brief 初始化已分配的 dict_t 结构体
 * @param d  待初始化的 dict_t 指针
 * @param ty 函数表指针
 * @return DICT_OK
 */
int _dict_init(dict_t* d, dict_func_t *ty);
/** @brief dict_init 是 _dict_init 的别名宏 */
#define  dict_init  _dict_init

/**
 * @brief 销毁字典内容（清空所有节点）但不释放 dict_t 本身
 * @param d dict_t 指针
 */
void dict_destroy(dict_t *d);

/**
 * @brief 添加键并返回节点指针（底层接口）
 *        若 key 已存在，通过 existing 返回已有节点指针；
 *        否则创建新节点插入字典并返回。
 * @param d        dict_t 指针
 * @param key      键指针
 * @param existing 输出：若键已存在则设置为已有节点指针（可传 NULL）
 * @return 新建节点指针；键已存在时返回 NULL 并设置 *existing
 */
dict_entry_t *dict_add_raw(dict_t *d, void *key, dict_entry_t **existing);

/**
 * @brief 添加或查找节点（若已存在则返回已有节点，否则插入新节点）
 * @param d   dict_t 指针
 * @param key 键指针
 * @return 已有或新建的 dict_entry_t 指针
 */
dict_entry_t *dict_add_or_find(dict_t *d, void *key);

/**
 * @brief 添加键值对并返回节点指针
 *        若 key 已存在则直接返回已有节点（不更新值）。
 * @param d   dict_t 指针
 * @param key 键指针
 * @param val 值指针
 * @return 新建或已有的 dict_entry_t 指针
 */
dict_entry_t* dict_add_get(dict_t* d, void* key, void* val);

/**
 * @brief 计算字节序列的大小写不敏感哈希值（djb2 变体）
 * @param buf 输入字节数组
 * @param len 字节数
 * @return 64 位哈希值
 */
uint64_t dict_gen_case_hash_function(const unsigned char *buf, size_t len);

/**
 * @brief 查找指定 key 的节点
 * @param d   dict_t 指针
 * @param key 键指针
 * @return 找到返回 dict_entry_t 指针；未找到返回 NULL
 */
dict_entry_t* dict_find(dict_t *d, const void *key);

/**
 * @brief 向字典添加键值对
 *        若 key 已存在则返回 DICT_ERR（不覆盖）。
 * @param d   dict_t 指针
 * @param key 键指针
 * @param val 值指针
 * @return 成功返回 DICT_OK；键已存在返回 DICT_ERR
 */
int dict_add(dict_t *d, void *key, void *val);

/**
 * @brief 手动扩容哈希表到至少能容纳 size 个元素
 * @param d    dict_t 指针
 * @param size 目标最小容量
 * @return 成功返回 DICT_OK；失败返回 DICT_ERR
 */
int dict_expand(dict_t *d, unsigned long size);

/**
 * @brief 计算字节序列的哈希值（SipHash 或 MurmurHash 变体）
 * @param key 输入数据指针
 * @param len 字节数
 * @return 64 位哈希值
 */
uint64_t dict_gen_hash_function(const void *key, size_t len);

/**
 * @brief 判断哈希表是否需要缩容（填充率低于 HASHTABLE_MIN_FILL）
 * @param dict dict_t 指针
 * @return 需要缩容返回 1；否则返回 0
 */
int ht_needs_resize(dict_t *dict);

/**
 * @brief 哈希表 resize 策略枚举
 */
typedef enum {
    DICT_RESIZE_ENABLE,  /**< 允许 resize */
    DICT_RESIZE_AVOID,   /**< 尽量避免 resize（如 fork 后） */
    DICT_RESIZE_FORBID,  /**< 强制禁止 resize */
} dict_resize_enable_enum;

/**
 * @brief 对字典执行缩容操作（将哈希表收缩到最小合适大小）
 * @param d dict_t 指针
 * @return 成功返回 DICT_OK；失败返回 DICT_ERR
 */
int dict_resize(dict_t *d);

/**
 * @brief 删除指定 key 的节点（调用析构函数释放键值）
 * @param ht  dict_t 指针
 * @param key 要删除的键指针
 * @return 成功返回 DICT_OK；未找到返回 DICT_ERR
 */
int dict_delete_key(dict_t *ht, const void *key);

/**
 * @brief 查找并返回指定 key 对应的值指针
 * @param d   dict_t 指针
 * @param key 键指针
 * @return 找到返回值指针；未找到返回 NULL
 */
void* dict_fetch_value(dict_t *d, const void *key);


/* ---- 迭代器接口 ---- */

/**
 * @brief 字典迭代器结构体
 *        实现 latte_iterator_pair_t 接口，支持安全/非安全两种遍历模式。
 *        安全迭代器（safe==1）：允许遍历期间调用 dict_add/dict_find 等操作。
 *        非安全迭代器（safe==0）：遍历期间仅允许调用 dict_next。
 */
typedef struct dict_iterator_t {
    latte_iterator_pair_t sup;       /**< 基类迭代器接口（has_next/next/release 回调） */
    long index;                      /**< 当前遍历的桶下标（-1 表示未开始） */
    int table;                       /**< 当前遍历的哈希表编号（0 或 1） */
    int safe;                        /**< 1 = 安全迭代器；0 = 非安全迭代器 */
    dict_entry_t *entry;             /**< 当前节点指针 */
    dict_entry_t *nextEntry;         /**< 下一个节点指针（防止当前节点被删除后丢失） */
    unsigned long long fingerprint;  /**< 非安全迭代器的指纹，用于误用检测 */
    int readed;                      /**< 已读取标志，用于迭代状态跟踪 */
} dict_iterator_t;

/**
 * @brief 检查字典迭代器是否还有下一个元素
 * @param it latte_iterator_t 指针（实际为 dict_iterator_t）
 * @return 有下一个元素返回 true；否则返回 false
 */
bool protected_dict_iterator_has_next(latte_iterator_t* it);

/**
 * @brief 释放字典迭代器
 * @param it latte_iterator_t 指针（实际为 dict_iterator_t）
 */
void protected_dict_iterator_delete(latte_iterator_t* it);

/**
 * @brief 获取字典迭代器的下一个键值对
 * @param it latte_iterator_t 指针（实际为 dict_iterator_t）
 * @return latte_pair_t 指针（含当前节点的 key/val）；无更多元素返回 NULL
 */
void* protected_dict_iterator_next(latte_iterator_t* it);

/**
 * @brief 获取字典的 latte 通用迭代器
 *        返回的迭代器以 key/val 键值对（latte_pair_t）形式遍历所有节点。
 * @param d dict_t 指针
 * @return 新建的 latte_iterator_t 指针；失败返回 NULL
 */
latte_iterator_t* dict_get_latte_iterator(dict_t *d);

/**
 * @brief 比较两个字典的内容（非对称性比较）
 *        检查字典 a 中的所有 key 是否在字典 b 中存在且值相等。
 *        注意：此函数具有非对称性，即
 *        private_dict_cmp(a, b) + private_dict_cmp(b, a) 不一定为 0。
 * @param a   第一个字典指针
 * @param b   第二个字典指针
 * @param cmp 值比较函数（void* 接口）
 * @return 相等返回 0；不等返回非零
 */
int private_dict_cmp(dict_t* a, dict_t* b, cmp_func cmp);
#endif
