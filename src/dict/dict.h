/*
 * dict.h - 哈希表 (字典) 头文件
 * 
 * Latte C 库核心组件：高性能哈希表实现
 * 
 * 设计参考：Redis 7.2.5
 * 
 * 核心特性：
 * 1. 渐进式 rehash - 避免单次扩容卡顿
 * 2. 双表结构 - 支持在线扩容/缩容
 * 3. 可定制回调 - hash/dup/compare/destructor
 * 4. 支持元数据 - 可扩展 entry 自定义数据
 * 5. 支持过期机制 - 预留 expire 功能
 * 
 * 主要结构：
 * - dict_t: 字典主结构 (含双哈希表)
 * - dict_entry_t: 键值对条目
 * - dict_func_t: 回调函数集
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef __LATTE_DICT_H
#define __LATTE_DICT_H

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include "iterator/iterator.h"
#include "cmp/cmp.h"

/* 返回值常量 */
#define DICT_OK 0          /* 操作成功 */
#define DICT_ERR 1         /* 操作失败 */

/* 未使用参数宏 (避免编译器警告) */
#define DICT_NOTUSED(V) ((void) V)

/*
 * 字典条目结构
 * 存储键值对，支持链表解决哈希冲突
 */
typedef struct dict_entry_t {
    void *key;             /* 键指针 */
    union {
        void *val;         /* 值指针 (通用类型) */
        uint64_t u64;      /* 无符号 64 位整数 (直接存储，避免分配) */
        int64_t s64;       /* 有符号 64 位整数 */
        double d;          /* 双精度浮点数 */
    } v;                   /* 值联合，节省内存 */
    struct dict_entry_t *next;  /* 链表指针，用于哈希冲突链 */
    void *metadata[];      /* 可变长元数据 (caller-defined)，按指针对齐 */
} dict_entry_t;

/* 前向声明 */
typedef struct dict_t dict_t;

/*
 * 字典回调函数集
 * 用于定制哈希表行为 (hash/dup/compare/destructor)
 */
typedef struct dict_func_t {
    /* 哈希函数：计算键的哈希值 */
    uint64_t (*hashFunction)(const void *key);
    
    /* 键复制函数：深拷贝键 (可选 NULL，表示浅拷贝) */
    void *(*keyDup)(dict_t *d, const void *key);
    
    /* 值复制函数：深拷贝值 (可选 NULL) */
    void *(*valDup)(dict_t *d, const void *obj);
    
    /* 键比较函数：比较两个键是否相等 */
    int (*keyCompare)(dict_t *d, const void *key1, const void *key2);
    
    /* 键释放函数：释放键内存 */
    void (*keyDestructor)(dict_t *d, void *key);
    
    /* 值释放函数：释放值内存 */
    void (*valDestructor)(dict_t *d, void *obj);
    
    /* 扩容允许函数：判断是否允许扩容 (moreMem - 需额外内存，usedRatio - 使用率) */
    int (*expandAllowed)(size_t moreMem, double usedRatio);
    
    /* 条目元数据大小：为每个 entry 预留自定义元数据空间 */
    size_t (*dictEntryMetadataBytes)(dict_t *d);
} dict_func_t;

/* 哈希表大小计算宏 */
#define DICTHT_SIZE(exp) ((exp) == -1 ? 0 : (unsigned long)1<<(exp))
#define DICTHT_SIZE_MASK(exp) ((exp) == -1 ? 0 : (DICTHT_SIZE(exp))-1)

/* 哈希表参数 */
#define HASHTABLE_MIN_FILL 10      /* 最小填充率 10%，低于此触发缩容 */

/* 计算总槽数 (双表之和) */
#define dict_slots(d) (DICTHT_SIZE((d)->ht_size_exp[0]) + DICTHT_SIZE((d)->ht_size_exp[1]))

/*
 * 字典主结构
 * 实现双哈希表，支持渐进式 rehash
 */
struct dict_t {
    dict_func_t *type;             /* 回调函数集指针 */

    /* 双哈希表指针 (用于 rehash)
     * ht_table[0]: 当前活跃表
     * ht_table[1]: rehash 目标表
     */
    dict_entry_t **ht_table[2];
    
    /* 两个表的已用槽数统计 */
    unsigned long ht_used[2];

    /* rehash 索引 (-1 表示未进行 rehash) */
    long rehashidx;
    
    /* 暂停 rehash 计数器 (>0 暂停，<0 表示错误) */
    int16_t pauserehash;
    
    /* 表大小指数 (size = 1<<exp)，-1 表示空表 */
    signed char ht_size_exp[2];
    
    /* 预留元数据空间 (用于过期时间等扩展功能) */
    void* metadata[];
};

/* 初始哈希表大小 */
#define DICT_HT_INITIAL_EXP 2      /* 初始指数 (1<<2 = 4 个槽) */
#define DICT_HT_INITIAL_SIZE (1<<(DICT_HT_INITIAL_EXP))  /* 初始大小 = 4 */

    /* Keep small vars at end for optimal (minimal) struct padding */
    int16_t pauserehash; /* If >0 rehashing is paused (<0 indicates coding error) */
    signed char ht_size_exp[2]; /* exponent of size. (size = 1<<exp) */

    void* metadata[]; //增加expire 针对单个subkey
};





/* This is the initial size of every hash table */
#define DICT_HT_INITIAL_EXP      2
#define DICT_HT_INITIAL_SIZE     (1<<(DICT_HT_INITIAL_EXP))


#define dict_free_val(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d), (entry)->v.val)


#define dict_free_key(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d), (entry)->key)

#define dict_set_key(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        (entry)->key = (d)->type->keyDup((d), _key_); \
    else \
        (entry)->key = (_key_); \
} while(0)

#define dict_compare_keys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d), key1, key2) : \
        (key1) == (key2))

#define dict_get_meta_data(entry) (&(entry)->metadata)
#define dict_get_meta_data_size(d) ((d)->type->dictEntryMetadataBytes \
                             ? (d)->type->dictEntryMetadataBytes(d) : 0)


#define dict_size(d) ((d)->ht_used[0]+(d)->ht_used[1])
#define dict_is_rehashing(d) ((d)->rehashidx != -1)
#define dict_hash_key(d, key) (d)->type->hashFunction(key)




#define dict_set_val(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        (entry)->v.val = (d)->type->valDup((d), _val_); \
    else \
        (entry)->v.val = (_val_); \
} while(0)

#define dict_pause_rehashing(d) (d)->pauserehash++
#define dict_resume_rehashing(d) (d)->pauserehash--

#define dict_get_entry_key(he) ((he)->key)
#define dict_get_entry_val(he) ((he)->v.val)
/* API */
dict_t* dict_new(dict_func_t *type);
void dict_delete(dict_t*d);

int _dict_init(dict_t* d, dict_func_t *ty);
#define  dict_init  _dict_init
void dict_destroy(dict_t*d);


dict_entry_t*dict_add_raw(dict_t*d, void *key, dict_entry_t**existing);
dict_entry_t*dict_add_or_find(dict_t*d, void *key);
dict_entry_t* dict_add_get(dict_t* d, void* key, void* val);
uint64_t dict_gen_case_hash_function(const unsigned char *buf, size_t len);

dict_entry_t* dict_find(dict_t*d, const void *key);
int dict_add(dict_t*d, void *key, void *val);
int dict_expand(dict_t*d, unsigned long size);


/* dict_tbasic function */
uint64_t dict_gen_hash_function(const void *key, size_t len);

int ht_needs_resize(dict_t*dict);
typedef enum {
    DICT_RESIZE_ENABLE,
    DICT_RESIZE_AVOID,
    DICT_RESIZE_FORBID,
} dict_resize_enable_enum;
int dict_resize(dict_t*d);
int dict_delete_key(dict_t*ht, const void *key);
void* dict_fetch_value(dict_t *d, const void *key);



//实现latte_iterator_t 替代原来的遍历
/* If safe is set to 1 this is a safe iterator, that means, you can call
 * dict_add, dict_find, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only dict_next()
 * should be called while iterating. */
typedef struct dict_iterator_t {
    latte_iterator_pair_t sup;
    // dict_t *d;
    long index;
    int table, safe;
    dict_entry_t *entry, *nextEntry;
    /* unsafe iterator fingerprint for misuse detection. */
    unsigned long long fingerprint;
    int readed;
} dict_iterator_t;

// dict_iterator_t *dict_get_iterator(dict_t*d);
// dict_entry_t*dict_next(dict_iterator_t *iter);
// void dict_iterator_delete(dict_iterator_t *iter);

bool protected_dict_iterator_has_next(latte_iterator_t* it);
void protected_dict_iterator_delete(latte_iterator_t* it);
void* protected_dict_iterator_next(latte_iterator_t* it);
latte_iterator_t* dict_get_latte_iterator(dict_t *d);

/** 
 * 非对称性
 *     比如 private_dict_cmp(a, b) + private_dict_cmp(b, a) != 0 
 */
int private_dict_cmp(dict_t* a, dict_t* b, cmp_func cmp);
#endif
