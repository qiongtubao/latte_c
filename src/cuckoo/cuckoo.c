#include "cuckoo.h"
#include <assert.h>
#include "error/error.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/*
 * Cuckoo Filter 实现 — 核心算法速览
 * -----------------------------------------------------------------------------
 * 单次 key 哈希 → 64 位 hv,从中拆分出:
 *   index = hv 高位 & (nbuckets - 1)   → 主候选桶 i1
 *   tag    = hv 低位 & ((1<<bits)-1)   → fingerprint(避免与 0 冲突)
 *   i2     = alt(i1, tag)              → 候选桶(由 tag 决定)
 *
 * 插入两阶段:
 *   1) no_kick:任意候选桶有 NULL 槽即放。
 *   2) kick_out:两桶皆满时随机踢出其中一个 tag,被踢 tag 走自己的 alt 路径;
 *      超过 MAX_ITERATION 仍未安置则暂存 victim cache。
 *   3) filter 级:跨表(新→旧)重复上述;最终触发 expand 在末表新增 4× 桶表。
 *
 * 查询/删除按"新表→旧表"顺序遍历,任意表命中即返回。
 */


/*
 * 判断 n 是否为 2 的幂。
 * 经典位运算:n 为 2 的幂当且仅当二进制只有一个 1,即 n & (n-1) == 0。
 * 特例:n == 0 时 n-1 全 1,结果为 0 但语义不对,因此额外排除。
 */
int is_pow_of_2(uint64_t n) { return (n & (n - 1)) == 0 && n != 0; }

/*
 * 计算大于或等于 n 的最小 2 的幂。
 * 算法("smear" 位运算):先把 n 减 1,再把最高位 1 之下所有位填成 1,最后 +1。
 * 例:n=5(101b)→ 减1=100b → smear=111b → +1=8(1000b)。
 * 边界:n==0 时 n-1 下溢,但由于返回结果恰好 0,语义仍可用(返回"最小 ≥0 的 2 幂"=0)。
 */
uint64_t upper_pow_of_2(uint64_t n) {
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n |= n >> 32;
	n++;
	return n;
}

/*
 * 估算首表桶数。
 * 思路:每桶 4 槽,若想容纳 estimated_keys 个 key,理想桶数 ≈ keys/4。
 * 向上对齐到 2 的幂是桶数要求,再保证不少于 MIN_BUCKETS。
 * 注意:这只是"初始"估算,运行时通过 expand 机制可继续增长。
 */
static size_t cuckoo_estimate_buckets(size_t estimated_keys) {
    size_t nbuckets = upper_pow_of_2(estimated_keys)/CUCKOO_FILTER_TAGS_PER_BUCKET; //2^n/4
    return nbuckets < CUCKOO_FILTER_TABLE_MIN_BUCKETS ? CUCKOO_FILTER_TABLE_MIN_BUCKETS : nbuckets; //[16,2^n/4]
}

/* tag 位宽枚举 → 实际位数的查找表。下标见 CUCKOO_FILTER_BITS_PER_TAG_*。 */
static int bits_per_tag_array[CUCKOO_FILTER_BITS_PER_TAG_TYPES] = {8,12,16,32};
/*
 * 用户传入枚举值,转为实际 tag 位宽。
 * 调试断言:防止越界访问 bits_per_tag_array。
 */
static inline int cuckoo_get_bits_per_tag(int bits_per_tag_type) {
    assert(bits_per_tag_type < CUCKOO_FILTER_BITS_PER_TAG_TYPES); //4种类型
    return bits_per_tag_array[bits_per_tag_type];
}
/*
 * 初始化一张 cuckoo 表。
 * 参数:
 *   bits_per_tag — 每 tag 占用位数(8/12/16/32)
 *   nbuckets    — bucket 数,必须为 2 的幂(便于 & (n-1) 取模)
 *
 * bytes_per_bucket 推导:
 *   4 个槽 × tag 位数 ÷ 8,向上取整为整字节数。
 *   8-bit  → 4 字节/bucket(每槽 1B,自然对齐)
 *   12-bit → 6 字节/bucket(每槽 1.5B,12 位紧凑打包)
 *   16-bit → 8 字节/bucket
 *   32-bit → 16 字节/bucket
 *
 * data 分配 nbuckets × bytes_per_bucket 字节并清零(zcalloc 兼顾分配与零初始化),
 * 保证所有槽的"初始 tag == CUCKOO_TAG_NULL (0)",省去显式 memset。
 */
void cuckoo_table_init(cuckoo_table_t *table, int bits_per_tag, size_t nbuckets) {
    size_t bytes_per_bucket =  (bits_per_tag*CUCKOO_FILTER_TAGS_PER_BUCKET+7)>>3; // (8 * 4 + 7) >> 3 => 4
    assert(is_pow_of_2(nbuckets));
    table->bits_per_tag = bits_per_tag;
    table->bytes_per_bucket = bytes_per_bucket;
    table->nbuckets = nbuckets;
    table->victim.used = 0;
    table->victim.index = 0;
    table->victim.tag = 0;
    table->ntags = 0;
    table->data = zcalloc(nbuckets*(bytes_per_bucket)); // 8M  * 4
}



/*
 * siphash-2-4 使用的 128 位密钥。
 * 写死在代码中(非随机生成),目的是让哈希分布稳定可重现。
 * 注意:若攻击者可控制输入且知道密钥,理论上可构造 hash collision —
 * 因此对外部 key 的场景建议改用进程启动时随机生成的密钥。
 */
static uint8_t cuckoo_hash_function_seed[16] =
{15, 228, 29, 66, 3, 163, 118, 182, 101, 208, 229, 232, 2, 74, 115, 47};

/* siphash 在 ../siphash/siphash.c 中实现,这里仅前置声明。 */
uint64_t siphash(const uint8_t *in, const size_t inlen, const uint8_t *k);

/* 默认哈希函数:siphash-2-4(64 位输出)。用作 cuckoo_filter_new 的便捷实参。 */
uint64_t cuckoo_gen_hash_function(const void *key, int len) {
    return siphash(key,len,cuckoo_hash_function_seed);
}
/*
 * 小端序检测:本实现在 write_tag 中按特定字节序直接读写 12-bit tag,
 * 因此仅在小端机(主流 x86/ARM)上正确,大端机上需要适配。
 */
static int is_littlen_endian() { int n = 1; return (*(char *)&n == 1); }

/*
 * 创建 cuckoo filter。
 * 流程:
 *   1. 校验机器字节序(仅小端支持)。
 *   2. 用 estimated_keys 估算首表桶数(cuckoo_estimate_buckets)。
 *   3. 分配 filter 与首张 table,完成 table_init 分配 data。
 * 初始 ntables = 1,后续插入触发扩容时再增长。
 */
cuckoo_filter_t* cuckoo_filter_new(cuckoo_hash_fn hash_fn, int bits_per_tag_type, size_t estimated_keys) {
    assert(is_littlen_endian());
    size_t nbuckets = cuckoo_estimate_buckets(estimated_keys);//[16,2^n/4]  => 8M
    cuckoo_filter_t *filter = zmalloc(sizeof(cuckoo_filter_t));
    filter->hash_fn = hash_fn;
    filter->bits_per_tag = cuckoo_get_bits_per_tag(bits_per_tag_type); //8
    filter->ntables = 1;
    filter->tables = zmalloc(sizeof(cuckoo_table_t));
    cuckoo_table_init(filter->tables, filter->bits_per_tag, nbuckets);
    return filter;
}

/* 释放单表的 data 缓冲并置 NULL。多次调用安全(idempotent)。 */
void cuckoo_table_deinit(cuckoo_table_t *table) {
    if (table->data) {
        zfree(table->data);
        table->data = NULL;
    }
}

/*
 * 释放整个 filter:遍历所有表释放 data,再释放表数组和 filter 自身。
 * 允许 filter == NULL。调用后指针不可再用。
 */
void cuckoo_filter_free(cuckoo_filter_t* filter) {
    if (filter == NULL) return;
    for (int i = 0; i < filter->ntables; i++) {
        cuckoo_table_deinit(filter->tables + i);
    }
    zfree(filter->tables);
    zfree(filter);
}

/* 包装 filter 的 hash_fn 调用,使上层无需关心签名细节。 */
static inline uint64_t cuckoo_filter_generate_hash(cuckoo_filter_t *filter,
        const char *key, size_t klen) {
    return filter->hash_fn(key,klen);
}

/*
 * 核心算法①:哈希切分 (Hash Splitting)
 * -----------------------------------------------------------------------------
 * 把一个 64 位 hv 拆成两个独立用途:
 *   i1  — 主候选桶索引(高位决定,落在 [0, nbuckets))
 *   tag — fingerprint(低位决定,占 bits_per_tag 位)
 *
 * 实现细节:
 *   i1 = (hv >> 32) & (nbuckets - 1)
 *       高 32 位足以覆盖 nbuckets(本实现 nbuckets ≤ 2^32),与运算比取模快。
 *   tag = hv 的低 32 位,截取低 bits_per_tag 位作为指纹。
 *
 * 关键不变量:tag != 0(因为 0 == CUCKOO_TAG_NULL,会被误判为空槽)。
 * 若算出的 tag == 0 则 +1,保证 tag 永远非零 — 这是 cuckoo filter
 * 能够"用 0 表示空槽"的常用技巧,使存储与"未使用"共享同一状态。
 */
static inline void cuckoo_table_index_tag(cuckoo_table_t* table, uint64_t hv,
        size_t *i1, uint32_t *tag) {
    *i1 = (hv >> 32) & (table->nbuckets -1); // 取高 32 位  & (2^n - 1) =>  (hv >> 32) & (8M (2^23) -1)  (取后23位)
    *tag = (hv & 0xFFFFFFFF) & ((1ULL << table->bits_per_tag) - 1); // 取低 32位 & （1 << 8-1 )。（取后8位）
    *tag += *tag == 0;
}

/*
 * 核心算法②:Alternate Index (备选桶号)
 * -----------------------------------------------------------------------------
 * 由主桶号 i1 和 tag 计算出该 key 可用的另一个候选桶号 i2。
 * 公式:
 *     i2 = (i1 XOR (tag * 0x5bd1e995)) & (nbuckets - 1)
 *
 * 关键性质:
 *   - 与运算替代模运算:nbuckets 是 2 的幂,只需保留低位,极快。
 *   - 对称性:把 i2 当 i1、代入相同公式可还原出原 i1
 *     (因为 XOR 与乘法都可逆),即 alt(i2, tag) == i1。
 *     这是 cuckoo filter 实现"踢出循环"的关键 — 被踢出的 tag 可以
 *     唯一确定地跳到它的另一桶。
 *
 * 常数 0x5bd1e995 (Mersenne prime 衍生):
 *   - 必须为奇数,保证乘法在二进制位上充分混合。
 *   - 与 2^32 互质 → tag × 常数 在 32 位空间内均匀散布,避免聚集。
 *   - 0101 1011 ... 0101 位模式 0/1 均衡,触发雪崩效应。
 *   - 出自 MurmurHash,被广泛复用为 cuckoo filter 的"b^k"散布项。
 */
static inline size_t cuckoo_table_alt_index(cuckoo_table_t* table, size_t i1, uint32_t tag) {
    return (i1 ^ ((size_t) tag * 0x5bd1e995)) & (table->nbuckets - 1);
}
/*
 * 核心算法③a:读出 bucket i 的第 j 个 tag。
 * -----------------------------------------------------------------------------
 * 4 种位宽的访问策略:
 *   - 8  bit:每槽 1 字节,直接数组下标访问。
 *   - 12 bit:每槽 12 位,4 槽共 48 位 = 6 字节紧凑打包。
 *             第偶数槽位于某 16 位半字的低 12 位,奇数槽位于高 12 位。
 *             读取时按 j 决定取低 12 位还是右移 4 位取高 12 位。
 *   - 16 bit:每槽 2 字节,直接数组下标访问。
 *   - 32 bit:每槽 4 字节,直接数组下标访问。
 *
 * 关键点:p 指向 bucket 起始字节;12-bit 分支需要先按 j 计算半字偏移
 *        (j + j>>1 = 1.5*j 字节),再用位运算取出目标 12 位。
 *        仅小端机器上成立。
 */
static inline
uint32_t cuckoo_table_read_tag(cuckoo_table_t* table, size_t i, size_t j) {
    assert(i < table->nbuckets && j < CUCKOO_FILTER_TAGS_PER_BUCKET);
    uint32_t tag = 0;
    uint8_t *p = table->data + i * table->bytes_per_bucket;
    if (table->bits_per_tag == 8) {
        tag = ((uint8_t*)p)[j];
    } else if (table->bits_per_tag == 12) {
        p += j + (j >> 1);
        tag = (*((uint16_t *)p) >> ((j & 1) << 2)) & 0xFFF;
    } else if (table->bits_per_tag == 16) {
        tag = ((uint16_t*)p)[j];
    } else if (table->bits_per_tag == 32) {
        tag = ((uint32_t*)p)[j];
    }
    return tag;
}

/*
 * 核心算法③b:把 tag 写入 bucket i 的第 j 个槽。
 * -----------------------------------------------------------------------------
 * 与 read_tag 对称。8/16/32-bit 直接数组赋值,12-bit 需要按"先清空目标 12 位,
 * 再写入新 12 位"的方式:
 *   - 偶数槽 (j&1==0):位于 16 位半字的低 12 位 → mask 0xf000 清空低 12 位。
 *   - 奇数槽 (j&1==1):位于 16 位半字的高 12 位 → mask 0x000f 清空高 12 位,
 *                       写入时整体左移 4 位。
 * 注意:tag 不会为 0(见 index_tag 中的 +1 调整),因此写入后读出一定 ≠ NULL。
 */
static inline
void cuckoo_table_write_tag(cuckoo_table_t* table, size_t i, size_t j, int32_t tag) {
    uint8_t *p = table->data + i * table->bytes_per_bucket;
    if (table->bits_per_tag == 8) {
        ((uint8_t*)p)[j] = tag;
    } else if (table->bits_per_tag == 12) {
        p += (j + (j >> 1));
        if ((j & 1) == 0) {
            ((uint16_t *)p)[0] &= 0xf000;
            ((uint16_t *)p)[0] |= tag;
        } else {
            ((uint16_t *)p)[0] &= 0x000f;
            ((uint16_t *)p)[0] |= (tag << 4);
        }
    } else if (table->bits_per_tag == 16) {
        ((uint16_t*)p)[j] = tag;
    } else if (table->bits_per_tag == 32) {
        ((uint32_t*)p)[j] = tag;
    }
}
/*
 * 核心算法④:无踢出插入 (Insert without kick-out)
 * -----------------------------------------------------------------------------
 * 路径:从 hv 拆出 (i1, tag),并算出 i2 = alt(i1, tag)。
 *   - 先扫 i1 的 4 个槽,找到第一个 NULL 槽即写入。
 *   - 否则扫 i2 的 4 个槽。
 * 任一桶找到空位 → 成功;两桶皆满 → 返回 CUCKOO_ERR(由调用方决定是否踢出或扩容)。
 *
 * 注意:本函数不检查"tag 已存在"的语义,因此同一 key 多次插入会消耗多个槽。
 * 这种"不去重"是 cuckoo filter 与其他 AMS 结构的差异之一:
 *   - 优点:无需额外数据结构,插入 O(1);
 *   - 缺点:对重复 key 浪费槽位,负载率可能虚高;FPP 不会因此变差。
 */
int cuckoo_table_insert_no_kick(cuckoo_table_t *table, uint64_t hv) {
    size_t i1, i2;
    uint32_t tag;

    cuckoo_table_index_tag(table, hv, &i1, &tag);
    i2 = cuckoo_table_alt_index(table, i1, tag);

    for (int j = 0; j < CUCKOO_FILTER_TAGS_PER_BUCKET; j++) {
        if (cuckoo_table_read_tag(table, i1, j) == CUCKOO_TAG_NULL) {
            cuckoo_table_write_tag(table, i1, j ,tag);
            table->ntags++;
            return CUCKOO_OK;
        }
    }

    for (int j = 0; j < CUCKOO_FILTER_TAGS_PER_BUCKET; j++) {
        if (cuckoo_table_read_tag(table, i2, j) == CUCKOO_TAG_NULL) {
            cuckoo_table_write_tag(table, i2, j, tag);
            table->ntags++;
            return CUCKOO_OK;
        }
    }

    return CUCKOO_ERR;
}

/* 取末表(最新、最大)的指针。filter 至少持有 1 张表,故 filter->tables 必非空。 */
static inline cuckoo_table_t* cuckoo_filter_current_table(cuckoo_filter_t* filter) {
    return &filter->tables[filter->ntables-1];
}

/*
 * 核心算法⑤:踢出循环插入 (Insert with kick-out)
 * -----------------------------------------------------------------------------
 * 当两个候选桶都满(no_kick 失败)时,需要"踢出"某个已存在的 tag 来腾位置。
 *
 * 算法步骤(每次外层迭代):
 *   1. 扫当前 bucket i 的 4 槽,有 NULL 即写入并返回成功(可能在前几次就退出)。
 *   2. 若全满,随机选一个槽 kj(& 3),把它的旧 tag 读出 otag,新 tag 替换入槽。
 *   3. 把 otag 视为"被踢出 tag",它在新位置 i' = alt(i, otag) 重新尝试。
 *   4. 如此链式反应,直到某次落到有空槽的桶或达到 MAX_ITERATION 上限。
 *
 * 随机踢出而非固定策略,目的是避免出现"周期性循环"(同一组 key 互相挤占
 * 同一桶);BFS/DFS 风格的固定策略在特定 key 集合上可能无限循环。
 *
 * Victim Cache 兜底:
 *   MAX_ITERATION 还没安置好 tag 时,把当前 (i, tag) 暂存到表的 victim cache。
 *   后续 contains/delete 会把 victim 也视为一个可能的命中位置。
 *   注意:每张表至多一个 victim,挤占即覆盖(后续 tag 把旧的挤掉);
 *         这种语义对正确性是安全的,只是极端情况下 victim 等于被覆盖的旧 victim。
 *
 * ntags 自增:成功路径 ntags++,失败进入 victim 也 ntags++,以反映"逻辑占用"。
 */
int cuckoo_table_insert_kick_out_index_tag(cuckoo_table_t* table, size_t i,
        uint32_t tag) {
    uint32_t otag;
    for (int loop = 0; loop < CUCKOO_FILTER_MAX_ITERATION; loop++) {
        for (int j = 0; j < CUCKOO_FILTER_TAGS_PER_BUCKET; j++) {
            if (cuckoo_table_read_tag(table,i,j) == CUCKOO_TAG_NULL) {
                cuckoo_table_write_tag(table,i,j,tag);
                table->ntags++;
                return CUCKOO_OK;
            }
        }

        size_t kj = rand() & (CUCKOO_FILTER_TAGS_PER_BUCKET - 1); //  & 3  随机一下
        otag = cuckoo_table_read_tag(table,i,kj);
        cuckoo_table_write_tag(table,i,kj,tag);
        tag = otag;
        i = cuckoo_table_alt_index(table,i,tag);
    }
    table->victim.used = 1;
    table->victim.index = i;
    table->victim.tag = tag;
    table->ntags++;

    return CUCKOO_OK;
}

/* kick_out 入口:从 hv 拆出 (i, tag) 后转交核心循环。 */
int cuckoo_table_insert_kick_out(cuckoo_table_t* table, uint64_t hv) {
    size_t i;
    uint32_t tag;
    cuckoo_table_index_tag(table, hv, &i, &tag);
    return cuckoo_table_insert_kick_out_index_tag(table, i, tag);
}

/*
 * 核心算法⑦:阶梯扩容 (Tiered Expansion)
 * -----------------------------------------------------------------------------
 * 当末表 kick_out 仍无法安置 tag 时(理论上仅在 MAX_ITERATION 用尽后才需要),
 * 在 filter 末尾追加一张新表,桶数 = 旧表 × BUCKETS_EXPANSION(= 4)。
 *
 * 步骤:
 *   1. 读末表 nbuckets,计算新桶数;超过 UINT32_MAX 时夹断。
 *   2. ntables++,把 tables 数组重分配到新长度(用 zrealloc,旧内容保留)。
 *   3. 重新指向末表(地址可能因 realloc 而变),调用 cuckoo_table_init
 *      完成新表的内存清零分配。
 *
 * 不变量:
 *   - ntables 永不超过 CUCKOO_FILTER_MAX_TABLES (= 4),超出返回 CUCKOO_ERR。
 *   - 旧表数据保持不变,不搬不迁移(查询/删除通过遍历所有表兼容)。
 *
 * 内存代价:每次扩容表数据增长 4×;连升 3 次后整体 ~64× 首表大小。
 * 不存在"缩容"路径,删除大量 key 也不会释放内存。
 */
static int cuckoo_filter_expand(cuckoo_filter_t* filter) {
    cuckoo_table_t* table = cuckoo_filter_current_table(filter);
    size_t nbuckets = table->nbuckets*CUCKOO_FILTER_BUCKETS_EXPANSION;
    if (filter->ntables >= CUCKOO_FILTER_MAX_TABLES)
        return CUCKOO_ERR;

    if (nbuckets > UINT32_MAX) nbuckets = UINT32_MAX;
    filter->ntables++;
    filter->tables = zrealloc(filter->tables,
            filter->ntables*sizeof(cuckoo_table_t));
    table = cuckoo_filter_current_table(filter);
    cuckoo_table_init(table, filter->bits_per_tag,nbuckets);
    return 0;
}
/*
 * 核心算法⑥:Filter 级插入编排 (Multi-table Insert Orchestration)
 * -----------------------------------------------------------------------------
 * 把单表 insert_no_kick / insert_kick_out 组合到多表 filter 上。
 *
 * 流程:
 *   1. 计算 key 哈希 hv。
 *   2. 从末表往前扫(新→旧),任一表的 no_kick 成功即返回(可能命中已存在 tag,
 *      也可能是新空位)。这一步实现了"优先放最新表" — 让旧表充当沉淀层。
 *   3. 所有表 no_kick 都失败 → 在末表执行 kick_out。
 *   4. kick_out 失败(MAX_ITERATION 用尽)→ 扩容并在新末表 no_kick。
 *   5. 扩容也失败(已达 MAX_TABLES)→ CUCKOO_ERR,filter 拒绝再插入。
 *
 * ⚠️ 当前实现的一处不一致:
 *   cuckoo_table_insert_kick_out 无论是否成功安置,都返回 CUCKOO_OK(失败时
 *   把 tag 暂存到 victim cache 也算成功)。这意味着第 3 步不会失败,
 *   进而第 4 步的 expand 在常规路径下是"惰性/几乎不可达"。
 *   实际扩容更多发生在 victim 已经被覆盖、表中所有候选桶+victim 都满的极端情况。
 *   是否要修改 kick_out 在 victim 满时返回 ERR,需结合业务负载评估。
 *
 * 时间复杂度:平均 O(1),最差 O(MAX_ITERATION × TAGS_PER_BUCKET)。
 */
int cuckoo_filter_insert(cuckoo_filter_t* filter, const char *key, size_t klen) {
    cuckoo_table_t* table;
    uint64_t hv = cuckoo_filter_generate_hash(filter, key, klen); //

    for (int i = filter->ntables-1; i >= 0; i--) { //最后table 往前遍历 如果有一个表存在 那就表示存在
        table = filter->tables + i;
        if (cuckoo_table_insert_no_kick(table, hv) == CUCKOO_OK)
            return CUCKOO_OK;
    }

    table = cuckoo_filter_current_table(filter); //最后一个table
    if (cuckoo_table_insert_kick_out(table, hv) == CUCKOO_OK)
        return CUCKOO_OK;

    if (cuckoo_filter_expand(filter) != CUCKOO_OK) {
        return CUCKOO_ERR;
    } else {
        table = cuckoo_filter_current_table(filter);
        return cuckoo_table_insert_no_kick(table, hv); //插入到最后table
    }
}

/*
 * 单表查询:key 是否"可能"在该表中。
 * -----------------------------------------------------------------------------
 * 流程:从 hv 拆出 (i1, tag),计算 i2 = alt(i1, tag)。
 *   1. 扫 i1 的 4 槽,任一等于 tag 即命中。
 *   2. 扫 i2 的 4 槽,同上。
 *   3. 都没命中,看 victim cache:若 victim.used 且 victim.tag == tag
 *      且 victim.index 等于 i1 或 i2(因为踢出循环中 tag 可能落在 i1/i2 的另一桶)。
 *
 * 假阳性来源:不同 key 哈希出相同 tag,正好落在某对桶之一。概率 ≈ 1/2^bits_per_tag。
 *
 * ⚠️ 注意事项(疑似 bug):第 466 行比较 `table->bits_per_tag == CUCKOO_FILTER_BITS_PER_TAG_8`
 *   其中 CUCKOO_FILTER_BITS_PER_TAG_8 == 0(类型枚举),而 table->bits_per_tag 是
 *   实际位宽(8/12/16/32),两者永远不会相等,因此该分支是死代码(永远不返回 ERR)。
 *   推测作者意图为 `bits_per_tag == 8` 但漏写了实际数值,或者早期 API 与现在不同。
 *   现状对正确性无影响(等价于该 if 不存在),但语义不清,建议清理。
 */
int cuckoo_table_contains(cuckoo_table_t* table, uint64_t hv) {
    size_t i1, i2;
    uint32_t tag;

    cuckoo_table_index_tag(table, hv, &i1, &tag);
    i2 = cuckoo_table_alt_index(table, i1, tag);

    if (table->bits_per_tag == CUCKOO_FILTER_BITS_PER_TAG_8) {
        return CUCKOO_ERR;
    }

    for (int j = 0; j < CUCKOO_FILTER_TAGS_PER_BUCKET; j++) {
        if (cuckoo_table_read_tag(table,i1,j) == tag) {
            return CUCKOO_OK;
        }
    }

    for (int j = 0; j < CUCKOO_FILTER_TAGS_PER_BUCKET; j++) {
        if (cuckoo_table_read_tag(table,i2,j) == tag) {
            return CUCKOO_OK;
        }
    }

    if (table->victim.used && table->victim.tag == tag &&
            (i1 == table->victim.index || i2 == table->victim.index)) {
        return CUCKOO_OK;
    }

    return CUCKOO_ERR;
}

/*
 * Filter 级查询:遍历所有表,任一表命中即返回 CUCKOO_OK。
 * 顺序为新→旧,与插入顺序一致 — 这保证新近插入的 key 优先匹配,
 * 在语义上不会因为扩容历史而错配。
 * 注意:cuckoo filter 永不返回 false negative(若真插入过,总能查到),
 * 但可能 false positive(未插入也可能命中)。
 */
int cuckoo_filter_contains(cuckoo_filter_t* filter, const char *key, size_t klen) {
    cuckoo_table_t* table;
    uint64_t hv = cuckoo_filter_generate_hash(filter,key,klen);

    for (int i = filter->ntables-1; i >= 0; i--) {
        table = filter->tables + i;
        if (cuckoo_table_contains(table,hv) == CUCKOO_OK)
            return CUCKOO_OK;
    }

    return CUCKOO_ERR;
}

/*
 * 尝试消化 victim cache:每次 delete 释放一个槽位后,趁此机会把 victim
 * 里的 tag 重新插回主表(走 kick_out 路径),从而:
 *   1. 释放 victim 槽,后续插入可继续使用。
 *   2. 把"暂存"的 tag 安置到主结构中,使其可被正常 delete。
 * 若 kick_out 又失败,则 tag 仍在 victim 中,函数依旧返回;这是安全降级。
 * 注意:此函数不会递归触发消除 — 即便刚释放的 victim 又被新踢出覆盖也无所谓。
 */
static inline void cuckoo_table_try_eliminate_victim_cache(cuckoo_table_t* table) {
    if (table->victim.used) {
        table->victim.used = 0;
        table->ntags--;
        cuckoo_table_insert_kick_out_index_tag(table, table->victim.index,
                table->victim.tag);
    }
}


/*
 * 单表删除:在 (i1, i2) 两个候选桶中查找 tag 并置 NULL。
 * 找到后释放 victim cache(把 victim 中的 tag 重新插入主表,可能腾出 slot)。
 * 若 tag 在 victim 中且 victim.index 等于 i1 或 i2,直接清空 victim。
 *
 * ⚠️ False deletion 风险:
 *   cuckoo filter 的 fingerprint(8/12/16 bit)有非零碰撞概率。如果两个不同 key
 *   的 tag 相同,且它们的 (i1, i2) 候选桶集合相同(实际只会发生当哈希巧合),
 *   删除其中一个会把另一个的 tag 也"清掉" — 即误删。
 *   tag 位数越少、负载率越高,误删概率越大;实际生产中通常可接受,但
 *   若用于 KV 存储的 tombstone,需要 key 二次确认。
 *
 * 返回:CUCKOO_OK 表示删除成功(可能误删),CUCKOO_ERR 表示未找到。
 */
int cuckoo_table_delete(cuckoo_table_t* table, uint64_t hv) {
    size_t i1, i2;
    uint32_t tag;

    cuckoo_table_index_tag(table,hv,&i1,&tag);
    i2 = cuckoo_table_alt_index(table,i1,tag);

    for (int j = 0; j < CUCKOO_FILTER_TAGS_PER_BUCKET; j++) {
        if (cuckoo_table_read_tag(table,i1,j) == tag) {
            cuckoo_table_write_tag(table,i1,j,CUCKOO_TAG_NULL);
            table->ntags--;
            cuckoo_table_try_eliminate_victim_cache(table);
            return CUCKOO_OK;
        }
    }

    for (int j = 0; j < CUCKOO_FILTER_TAGS_PER_BUCKET; j++) {
        if (cuckoo_table_read_tag(table,i2,j) == tag) {
            cuckoo_table_write_tag(table,i2,j,CUCKOO_TAG_NULL);
            table->ntags--;
            cuckoo_table_try_eliminate_victim_cache(table);
            return CUCKOO_OK;
        }
    }

    if (table->victim.used && table->victim.tag == tag &&
        (i1 == table->victim.index || i2 == table->victim.index)) {
        table->victim.used = 0;
        table->ntags--;
        return CUCKOO_OK;
    }

    return CUCKOO_ERR;
}

/*
 * Filter 级删除:遍历所有表(新→旧),任一表 table_delete 成功即返回。
 * 一旦找到并删除首个匹配的 tag 立刻返回,后续表中相同 tag 不再处理。
 * 注意:这只删除了一个匹配项,若同 key 被插入多次需多次 delete。
 */
int cuckoo_filter_delete(cuckoo_filter_t* filter, const char *key, size_t klen) {
    cuckoo_table_t* table;
    uint64_t hv = cuckoo_filter_generate_hash(filter, key, klen);

    for (int i = filter->ntables-1; i >= 0; i--) {
        table = filter->tables+i;
        if (cuckoo_table_delete(table, hv) == CUCKOO_OK)
            return CUCKOO_OK;
    }
    return CUCKOO_ERR;
}

/*
 * 统计 filter 的整体使用情况:
 *   - ntags:        所有表已用 tag 总数(不含 victim)
 *   - used_memory:  所有表 data 占用的字节数总和
 *   - ntables:      当前持有的表数
 *   - load_factor:  整体负载率 = ntags / (各表 slots 之和)
 *   - load_factors: 每张表各自的负载率,便于观察扩容时机
 * 注意:整体负载率往往较低(因新表才刚创建、负载接近 0),
 *       判断"是否需要扩容"应看最末表的负载率(通常 > 0.9 触发踢出循环)。
 */
void cuckoo_filter_get_stat(cuckoo_filter_t* filter, cuckoo_filter_stat_t* stat) {
    size_t total_slots = 0;
    memset(stat,0,sizeof(cuckoo_filter_stat_t));
    stat->ntables = filter->ntables;
    for (int i = 0; i <filter->ntables; i++) {
        cuckoo_table_t* table = filter->tables+i;
        size_t slots = table->nbuckets*CUCKOO_FILTER_TAGS_PER_BUCKET;
        stat->ntags += table->ntags;
        stat->used_memory += table->bytes_per_bucket * table->nbuckets;
        stat->load_factors[i] = (double)table->ntags / slots;
        total_slots += slots;
    }
    stat->load_factor = (double)stat->ntags / total_slots;
}

/* 返回 filter 所有表 data 占用的总字节数(不含 filter/table 结构体本身)。 */
size_t cuckoo_filter_used_memory(cuckoo_filter_t* filter) {
    size_t used_memory = 0;
    for (int i = 0; i < filter->ntables; i++) {
        cuckoo_table_t* table = filter->tables+i;
        used_memory += table->bytes_per_bucket * table->nbuckets;
    }
    return used_memory;
}

