#ifndef __LATTE_CUCKOO_H
#define __LATTE_CUCKOO_H

/*
 * Cuckoo Filter (布谷鸟过滤器)
 * -----------------------------------------------------------------------------
 * 一种支持插入、查询、删除的近似成员判定 (Approximate Membership) 数据结构。
 * 与 Bloom Filter 相比:
 *   - 支持 delete(因为存的是 fingerprint 而不是 hash bit)
 *   - 更高的空间利用率(典型 95% 负载率 vs Bloom 的 ~50%)
 *   - 假阳性率 (FPP) ≈ 1 / 2^bits_per_tag(8-bit tag ≈ 0.4%)
 *
 * 本实现采用"多表阶梯扩容"策略:filter 由多张表组成,新表桶数为旧表的 4 倍,
 * 旧表只读不搬,扩容靠新增更大表承载新增 key。
 *
 * 核心思想(单表):
 *   1. 对 key 做一次哈希得到 64 位 hv。
 *   2. 取高 32 位算 bucket index i1,取低 32 位取模得 fingerprint tag。
 *   3. 每个 bucket 有 4 个 tag 槽;key 也可被放到 i2 = alt(i1, tag) 这一桶。
 *   4. 插入时若两桶皆满则随机踢出一个旧 tag,被踢 tag 放到它的 alt 桶,
 *      形成链式反应;超过最大迭代次数则触发扩容。
 */
#include <stdint.h>
#include <stddef.h>


/* 操作结果码:CUCKOO_OK 成功,CUCKOO_ERR 失败(扩容已满、tag 不存在等) */
#define CUCKOO_OK 0
#define CUCKOO_ERR -1

/* 未使用参数消除编译告警 */
#define CUCKOO_UNUSED(V) ((void) V)

/*
 * 踢出循环最大迭代次数。
 * 超过此数仍未找到空槽,视为当前表已满,触发扩容或写入 victim cache。
 * 经验值:500 是负载率/性能折中点;调小可减少最长插入耗时,调大可推迟扩容。
 */
#define CUCKOO_FILTER_MAX_ITERATION 500

/* 每个 bucket 可存放的 tag 数量(fingerprint 槽位数)。标准 cuckoo filter 取 4。 */
#define CUCKOO_FILTER_TAGS_PER_BUCKET  4

/* 扩容倍数:每次新增表桶数 = 旧表桶数 × 该值。 */
#define CUCKOO_FILTER_BUCKETS_EXPANSION 4

/* 多表模式下 filter 最多持有的表数。1 + 4 + 16 + 64 = 85 倍首表容量封顶。 */
#define CUCKOO_FILTER_MAX_TABLES 4

/* 空 tag 标记:全 0 即"该槽无 fingerprint"。注意:计算 tag 时若天然为 0 会 +1,避免与空槽混淆。 */
#define CUCKOO_TAG_NULL 0

/* 首表最小桶数。防止 estimated_keys 过小时桶数太小、负载率难以稳定。
 * 也是 cuckoo_filter_shrink 的缩容下限。 */
#define CUCKOO_FILTER_TABLE_MIN_BUCKETS  16

/* 用户传入的 tag 位宽枚举(数组下标)。实际位宽见 bits_per_tag_array。 */
#define CUCKOO_FILTER_BITS_PER_TAG_8  0
#define CUCKOO_FILTER_BITS_PER_TAG_12 1
#define CUCKOO_FILTER_BITS_PER_TAG_16 2
#define CUCKOO_FILTER_BITS_PER_TAG_32 3
#define CUCKOO_FILTER_BITS_PER_TAG_TYPES 4



/* 用户提供的哈希函数签名:输入 key 及其长度,输出 64 位 hash 值。
 * 推荐使用 siphash 类带密钥的哈希,避免 hash flooding。 */
typedef uint64_t (*cuckoo_hash_fn)(const void *key, int klen);

/*
 * Victim Cache (受害者缓存) — 踢出失败时的兜底槽。
 * 每张表最多保留一个"踢出循环"也没地方放的 tag。
 * 不变量:used == 0 时整个结构无意义;
 *        used == 1 时 (index, tag) 代表被暂存的那一个 fingerprint。
 * 任何新插入若直接命中该槽对应的两个候选桶,则认为该 key 已存在。
 *
 * used 还兼任"这张表已满"的标志位:一旦为 1,
 * cuckoo_table_insert_kick_out_index_tag 会直接返回 CUCKOO_ERR 而不再尝试,
 * 由上层触发扩容。这样 victim 里的 tag 永远不会被后来者覆盖,
 * 从而保住 filter "不产生 false negative" 的承诺。
 * 该标志只在 delete 成功腾出空槽时,由 try_eliminate_victim_cache 复位。
 */
typedef struct cuckoo_victim_cache_t {
    int used;        /* 是否有暂存的 tag */
    uint32_t tag;    /* 暂存的 fingerprint */
    size_t index;    /* 暂存 tag 对应的桶索引 */
} cuckoo_victim_cache_t;

/*
 * 单张 cuckoo 表。
 * data 的内存布局:nbuckets 个 bucket 连续排列,每个 bucket 包含
 * CUCKOO_FILTER_TAGS_PER_BUCKET 个 tag 槽,按 bits_per_tag 位宽紧凑存储:
 *   -  8-bit tag: 每个槽 1 字节,4 字节/bucket
 *   - 12-bit tag: 每槽 12 位,3 字节容纳 2 个 tag,6 字节/bucket
 *   - 16-bit tag: 每个槽 2 字节,8 字节/bucket
 *   - 32-bit tag: 每个槽 4 字节,16 字节/bucket
 * 不变量:nbuckets 必须是 2 的幂;bytes_per_bucket 已在 init 时算好。
 */
typedef struct cuckoo_table_t {
    size_t bits_per_tag;         /* tag 位宽(8/12/16/32) */
    size_t bytes_per_bucket;     /* 单 bucket 字节数 */
    size_t nbuckets;             /* bucket 数,必须为 2 的幂 */
    cuckoo_victim_cache_t victim;/* 踢出失败的兜底槽 */
    size_t ntags;                /* 当前已用 tag 数(不包含 victim) */
    uint8_t *data;               /* nbuckets × bytes_per_bucket 字节 */
} cuckoo_table_t;

/*
 * Cuckoo Filter 主结构 — 多表阶梯式布局。
 * tables 数组长度 = ntables,索引越大越新,容量也越大(4 倍递增)。
 * 关键设计:
 *   - 插入按 ntables-1 → 0 遍历,优先放最新表;旧表充当"已沉淀"区。
 *   - 不搬迁旧表数据,扩容只新增末表。
 *   - contains/delete 同样按新→旧顺序遍历;找到即可返回。
 * 这意味着 filter 内存只增不减,delete 仅释放 tag,不回收表。
 */
typedef struct cuckoo_filter_t {
    cuckoo_hash_fn hash_fn;     /* 用户提供的哈希函数 */
    int bits_per_tag;           /* 所有表统一使用的 tag 位宽 */
    int ntables;                 /* 当前持有的表数,1..CUCKOO_FILTER_MAX_TABLES */
    cuckoo_table_t *tables;      /* 表数组,ntables 项,末项最大 */
} cuckoo_filter_t;

/*
 * 统计信息结构。
 * load_factors[i] 为第 i 张表当前的负载率;load_factor 为整体加权负载率。
 * 注意整体负载率不等于各表平均,因为新表大,主导整体。
 */
typedef struct cuckoo_filter_stat_t {
  size_t ntags;                /* 所有表已用 tag 总和(不含 victim) */
  size_t used_memory;          /* 所有表 data 占用的总字节数 */
  size_t ntables;              /* 当前表数 */
  double load_factor;          /* 整体负载率 */
  double load_factors[CUCKOO_FILTER_MAX_TABLES]; /* 每张表的负载率 */
} cuckoo_filter_stat_t;


/* 内置默认哈希函数:siphash-2-4 + 内置 16 字节密钥。无法注入外部状态。 */
uint64_t cuckoo_gen_hash_function(const void *key, int len);

/*
 * 创建 cuckoo filter。
 *   hash_fn:            哈希函数(siphash 或其他 64-bit 哈希)
 *   bits_per_tag_type:  CUCKOO_FILTER_BITS_PER_TAG_8/12/16/32 之一
 *   estimated_keys:     期望容纳的 key 数;决定首表桶数
 * 返回初始化完成的 filter;失败返回 NULL。
 */
cuckoo_filter_t* cuckoo_filter_new(cuckoo_hash_fn hash_fn, int bits_per_tag_type, size_t estimated_keys);

/* 释放 filter 的所有表与自身。允许 filter == NULL。 */
void cuckoo_filter_free(cuckoo_filter_t* cuckoo);

/*
 * 插入一个 key。
 * 返回 CUCKOO_OK 表示插入成功或 key 已存在(本实现不去重,语义按"已存在则无效插入");
 * 返回 CUCKOO_ERR 表示扩容到 CUCKOO_FILTER_MAX_TABLES 上限后仍无法安置。
 */
int cuckoo_filter_insert(cuckoo_filter_t* filter, const char *key, size_t klen);

/*
 * 查询 key 是否可能存在(可能有假阳性,不可能有假阴性)。
 * 返回 CUCKOO_OK 表示"可能存在",CUCKOO_ERR 表示"一定不存在"。
 */
int cuckoo_filter_contains(cuckoo_filter_t* filter, const char *key, size_t klen);

/*
 * 删除一个 key。注意 cuckoo filter 的 delete 不保证幂等性:
 * 如果同一个 fingerprint 但不同 key 在同一对桶共存,删除一个可能误删另一个
 * (即所谓"false deletion")。FPP 越高,该风险越大。
 * 返回 CUCKOO_OK 表示删除成功,CUCKOO_ERR 表示未找到。
 */
int cuckoo_filter_delete(cuckoo_filter_t* filter, const char *key, size_t klen);

/*
 * 缩容 —— cuckoo_filter_expand 的逆操作。**一次调用只处理一张表。**
 * 没有任何策略参数:说缩就缩,缩不动就返回 CUCKOO_ERR。
 *
 *   CUCKOO_OK  — 缩掉了一张表(或把唯一的表就地缩小了一档)
 *   CUCKOO_ERR — 缩不动。filter 保持原样,一个字节都没变。
 *
 * 想缩到底就循环调用:
 *
 *     while (cuckoo_filter_shrink(filter) == CUCKOO_OK) ;
 *     // 需要知道省了多少就自己前后比 cuckoo_filter_used_memory()
 *
 * 两条互斥分支,都要求 ntables > 1:
 *   A. 末表为空 → 直接摘掉,零迁移成本,必定成功
 *   B. 末表非空 → 把末表的 tag 并入前一张表,再摘掉末表
 *
 * ⚠️ 只摘表,绝不动基表。ntables == 1 时直接返回 CUCKOO_ERR,理由:
 *   - tables[0] 的尺寸是调用方通过 estimated_keys 声明的容量意图;
 *   - 它是阶梯的基准 —— expand 用"末表 × 4"算新表大小,改小了基表就再也
 *     长不回原来的 nb0 / 4nb0 / 16nb0 / 64nb0,可逆性丢失,
 *     4 张表的总容量上限也被永久压低;
 *   - 缩基表不减少 ntables,对查询开销和 FPP 的表数因子毫无贡献。
 * 想回收基表那部分内存只能重建 filter(用更小的 estimated_keys 新建并
 * 重新插入还活着的 key),那是调用方的决策,不该由缩容偷偷做。
 *
 * 对查询没有影响:tag 迁进老表后仍然落在它自己的合法候选桶里,
 * contains 照样找得到(不会产生 false negative),而且要扫的表少了一张。
 *
 * 为什么是"合并末表"而不是"每张表各自瘦身":
 *   contains / delete 是 O(ntables) 逐表遍历,假阳性率也随表数近似线性上升
 *   (FPP ≈ 1-(1-1/(2^f-1))^(8×ntables),8-bit 实测 1/2/3/4 张表分别约
 *   3.0% / 6.0% / 8.9% / 11.7%)。表数才是主导代价,各表就地瘦身动不了它。
 *   而且合并末表能守住 1:4:16:64 的等比阶梯,下一次 expand 会精确重建
 *   刚摘掉的那张表,严格可逆。
 *
 * 为什么方向上做得到:
 *   桶号是 (hv >> 32) & (nbuckets - 1),目标表桶数是源表的 1/4,
 *   新桶号就是旧桶号的低位截断,光凭表内信息即可算出;
 *   反过来扩容需要已经丢掉的高位,无法恢复。迁移时也不需要知道 tag 当前
 *   在主桶还是备桶 —— 由 alt() 的对称性,两种情况推出的候选桶集合
 *   { i & M', (i ^ h) & M' } 完全相同。推导见 cuckoo.c。
 *   注意这和"把所有表合并成一张"不同:后者要求目标 ≤ 所有源表中最小的
 *   那张(tables[0]),通常装不下,确实不可行。
 *
 * 语义保证:
 *   - 事务性:每个分支要么完成,要么原状态一个字节都不变,绝不丢 tag。
 *     因此调用前后都不会产生 false negative。
 *   - 至少保留一张表(filter 必须始终持有 current table)。
 *   - 内存和假阳性率之间没有免费午餐:摘表会降低 FPP 的表数因子,
 *     但目标表负载率会上升。缩得越狠 FPP 越高,这是结构决定的。
 *   - 只搬运,不做语义清理:既不去重,也不会清掉历史误删留下的陈旧 tag。
 *
 * 注意:
 *   - 分支 B/C 是一次全量重建,开销 O(末表桶数),不要在写入高峰期反复调用;
 *     适合"批量删除后进入稳定期"时手动触发。
 *   - 只有 O(1) 的硬容量预检(装不下就不试),没有软阈值。所以数据较密时
 *     可能白做一次 O(桶数) 的迁移才发现放不下,然后回滚返回 CUCKOO_ERR。
 */
int cuckoo_filter_shrink(cuckoo_filter_t* filter);

/* 读取 filter 的统计信息,见 cuckoo_filter_stat_t。 */
void cuckoo_filter_get_stat(cuckoo_filter_t* filter, cuckoo_filter_stat_t* stat);

/* 返回 filter 当前占用的 data 字节数总和(不含结构体本身)。 */
size_t cuckoo_filter_used_memory(cuckoo_filter_t* filter);

/* 工具:判断 n 是否为 2 的幂。n == 0 返回 0。 */
int is_pow_of_2(uint64_t n);
/* 工具:返回 ≥ n 的最小 2 的幂。n == 0 时返回 0;否则返回 1,2,4,8,... */
uint64_t upper_pow_of_2(uint64_t n);
#endif
