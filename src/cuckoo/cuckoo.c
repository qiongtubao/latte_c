#include "cuckoo.h"
#include <assert.h>
#include "error/error.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "debug/latte_debug.h"

/* =============================================================================
 *                      Cuckoo Filter (布谷鸟过滤器) 实现
 * =============================================================================
 *
 * ## 0. 先搞清楚这东西是干什么的
 *
 * 它回答一个问题:"这个 key 我以前见过吗?"
 *
 * 但它不存 key 本身。它只存 key 的一小段"指纹"(fingerprint,代码里叫 tag),
 * 比如 8 个 bit。所以:
 *
 *   - 它说"不存在" → 100% 可信,一定没插入过。(不会有 false negative)
 *   - 它说"存在"   → 可能是别人的指纹撞上了。(会有 false positive,约 1/2^8 ≈ 0.4%)
 *
 * 为什么要这么省?因为省内存。存 100 万个 key,真实 key 可能几十 MB,
 * 用 8-bit 指纹只需要约 1 MB。典型用途:
 *   - 缓存前置:先问过滤器,说"不存在"就不用去查磁盘/网络了。
 *   - 去重:判断这条消息是不是重复投递。
 *
 * 和 Bloom Filter 比,它的两个卖点:
 *   1. 支持删除(Bloom 不能删,因为多个 key 共享同一个 bit)。
 *   2. 空间利用率更高(能塞到 95% 满还工作良好)。
 *
 *
 * ## 1. 数据长什么样(内存布局)
 *
 * 一张表(table)= 一个大数组,被切成 nbuckets 个"桶"(bucket)。
 * 每个桶有 4 个"槽"(slot),每个槽放一个 tag。
 *
 *     bucket 0        bucket 1        bucket 2       ...
 *   ┌──┬──┬──┬──┐  ┌──┬──┬──┬──┐  ┌──┬──┬──┬──┐
 *   │3A│00│7F│00│  │00│00│00│00│  │C1│C1│09│2B│  ...     (以 8-bit tag 为例)
 *   └──┴──┴──┴──┘  └──┴──┴──┴──┘  └──┴──┴──┴──┘
 *     ↑        ↑                     ↑
 *   有指纹   00 = 空槽              两个相同 tag 可以共存(不去重)
 *
 * 约定:tag == 0 表示"这个槽是空的"。所以真实 tag 永远不允许等于 0
 *      (见 cuckoo_table_index_tag,算出 0 就 +1 变成 1)。
 *      好处:分配内存时 zcalloc 清零,所有槽自动就是"空",不用额外初始化。
 *
 *
 * ## 2. 一个 key 能放在哪儿?(两个候选桶)
 *
 * 对 key 做一次哈希,得到 64 位的 hv,然后从这一个数里榨出三样东西:
 *
 *     hv  = ┌──────── 高 32 位 ────────┬──────── 低 32 位 ────────┐
 *           │  用来算桶号 i1           │  用来算指纹 tag          │
 *           └──────────────────────────┴──────────────────────────┘
 *
 *     i1  = (hv >> 32) & (nbuckets - 1)      主候选桶
 *     tag = (hv低32位) & ((1<<bits) - 1)     指纹,且保证非 0
 *     i2  = alt(i1, tag)                     备选候选桶(由 i1 和 tag 算出)
 *
 * 关键:每个 key 有 2 个候选桶(i1 和 i2),每桶 4 个槽 → 一共 8 个合法位置。
 * 查询时只需要看这 2 个桶 = 最多 2 次内存访问,非常快。
 *
 *
 * ## 3. 名字里的"布谷鸟"是什么意思?(踢出 / kick-out)
 *
 * 布谷鸟会把别人的蛋踢出巢,把自己的蛋放进去。这里也一样:
 *
 *   插入 X,发现 i1 和 i2 的 8 个槽全满了怎么办?
 *   → 随机挑一个槽,把里面的老 tag(叫它 Y)拿出来,把 X 写进去。
 *   → 现在 Y 流浪了,但 Y 还有另一个候选桶!把 Y 送去 alt(当前桶, Y)。
 *   → 如果那边也满,再踢一个 Z 出来……链式反应。
 *   → 最多折腾 CUCKOO_FILTER_MAX_ITERATION (500) 次。
 *
 * 这能成立,靠的是 alt() 的"对称性":alt(alt(i, tag), tag) == i。
 * 也就是说,一个 tag 无论现在在哪个桶,都能唯一算出"我的另一个家在哪"。
 *
 *   插入 X,两桶全满:
 *
 *     i1 [A][B][C][D]        随机踢 B          i1 [A][X][C][D]
 *     i2 [E][F][G][H]   ──────────────────►    B 去 alt(i1, B) 找位置
 *                                                   │
 *                                              还满? 再踢一个,继续…
 *
 * 500 次还没安置好 → 塞进 victim cache(每张表一个应急槽,见下)。
 *
 *
 * ## 4. Victim Cache(受害者缓存)
 *
 * 踢了 500 次还流浪的那个 tag,不能直接丢(丢了就产生 false negative,
 * 破坏"说不存在就一定不存在"的保证)。所以单独用一个字段存起来:
 *     victim = { used, index, tag }
 * 查询和删除时,除了扫 2 个桶,还要顺便看一眼 victim 是否匹配。
 *
 * 每张表只有一个 victim 槽,而且它绝不会被覆盖:
 * kick_out 一进门就检查"victim 是不是已经被占了",占了就直接拒绝插入
 * (返回 CUCKOO_ERR),把决定权交给上层去扩容。
 * 所以 victim 同时扮演两个角色:
 *   1. 兜底存储 —— 放那个实在安置不下的 tag。
 *   2. "这张表满了"的标志位 —— 一旦被占,这张表就不再接受踢出插入了。
 *
 *
 * ## 5. 满了怎么扩容?(本实现的特色:多表阶梯)
 *
 * 常规 cuckoo filter 扩容要 rehash 全部数据,很贵。本实现的做法是"加一层":
 *
 *     tables[0]  16 桶     ← 最老,只读不搬
 *     tables[1]  64 桶
 *     tables[2] 256 桶
 *     tables[3]1024 桶     ← 最新,新数据优先往这里放
 *
 * 每张新表是上一张的 CUCKOO_FILTER_BUCKETS_EXPANSION (4) 倍,
 * 最多 CUCKOO_FILTER_MAX_TABLES (4) 张。旧表数据原地不动,零搬迁成本。
 *
 * 代价:查询/删除要遍历所有表(最多 4 张),即最多 4×2 = 8 个桶。
 * 遍历顺序统一是"新表 → 旧表",因为新数据在新表,命中概率更高。
 *
 *
 * ## 6. 三个操作的完整路径速查
 *
 *   insert:  新→旧 每张表试 no_kick(找现成空槽)
 *            → 都失败,在末表 kick_out(踢出链;末表 victim 已占用则直接拒绝)
 *            → 还失败,expand 加新表再 no_kick
 *            → 还失败(表数已达上限),返回 CUCKOO_ERR
 *
 *   contains:新→旧 每张表看 i1 的 4 槽 / i2 的 4 槽 / victim,命中即返回 OK
 *
 *   delete:  新→旧 每张表找到 tag 就写回 0,并顺手把 victim 重新插一次
 *
 *
 * ## 7. 使用前必须知道的三个"坑"
 *
 *   a) 不去重:同一个 key 插 2 次会占 2 个槽,删 1 次只删掉 1 个。
 *   b) 误删(false deletion):两个不同 key 指纹相同且候选桶相同时,
 *      删 A 会把 B 的记录也抹掉 → B 会变成"查不到"。要求强正确性的场景
 *      必须用真 key 做二次确认。
 *   c) 只增不减:delete 只把槽置 0,不会释放表内存,也没有缩容路径。
 *
 * ## 8. 平台限制
 *
 *   12-bit tag 的紧凑打包直接按小端字节序读写内存,所以整个实现
 *   目前只在小端机器(x86 / 主流 ARM)上正确。见 is_littlen_endian 的断言。
 * =============================================================================
 */


/* -----------------------------------------------------------------------------
 * 判断 n 是不是 2 的幂(1, 2, 4, 8, 16, ...)。
 *
 * 原理:2 的幂的二进制里只有一个 1。减 1 会把那个 1 变成 0、后面全变成 1,
 *      于是两者相与必然为 0。
 *
 *      n     = 8 = 1000b
 *      n - 1 = 7 = 0111b
 *      n & (n-1) = 0000b  → 是 2 的幂 ✓
 *
 *      n     = 6 = 0110b
 *      n - 1 = 5 = 0101b
 *      n & (n-1) = 0100b ≠ 0 → 不是 ✗
 *
 * 特例:n == 0 时 n-1 = 全 1(无符号下溢),相与也是 0,但 0 显然不是 2 的幂,
 *      所以必须额外加上 n != 0 这一条。
 *
 * 为什么全篇都在纠结 2 的幂?因为桶数是 2 的幂时,"对桶数取模"可以写成
 * `x & (nbuckets - 1)`,一条 AND 指令搞定,比除法快一个数量级。
 * -------------------------------------------------------------------------- */
int is_pow_of_2(uint64_t n) { return (n & (n - 1)) == 0 && n != 0; }

/* -----------------------------------------------------------------------------
 * 返回 ≥ n 的最小的 2 的幂。例:5→8,8→8,9→16,1000→1024。
 *
 * 算法俗称 "bit smearing"(把最高位的 1 向低位涂抹开):
 *   第 1 步 n--        :让本来已经是 2 的幂的输入不会被推到下一档
 *                       (8 应该返回 8 而不是 16)。
 *   第 2 步 一串 |=    :把最高位 1 右边的所有位都刷成 1。
 *                       右移 1、2、4、8、16、32 累加起来正好能覆盖 64 位,
 *                       每一轮让"已经变成 1 的区域"长度翻倍。
 *   第 3 步 n++        :全 1 的数 +1 就进位成了一个干净的 2 的幂。
 *
 * 走一遍 n = 5:
 *   n-- → 4      = 0000 0100
 *   n |= n>>1    = 0000 0110
 *   n |= n>>2    = 0000 0111   (此时最高位 1 以下已全为 1)
 *   后面几步不再改变
 *   n++ → 8      = 0000 1000   ✓
 *
 * 边界:n == 0 时 n-- 下溢成全 1,涂抹后仍是全 1,+1 回绕成 0。
 *      刚好可以理解成"≥0 的最小 2 的幂 = 0",不会崩,但调用方别依赖这个。
 * -------------------------------------------------------------------------- */
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

/* -----------------------------------------------------------------------------
 * 根据"预计要装多少 key"估算第一张表需要多少个桶。
 *
 * 推导:每个桶 4 个槽 → 装 N 个 key 至少要 N/4 个桶。
 *      桶数必须是 2 的幂(为了用 & 代替 %),所以先把 N 向上取整到 2 的幂再除 4。
 *      最后兜一个下限 CUCKOO_FILTER_TABLE_MIN_BUCKETS (16),
 *      否则 estimated_keys 很小时(比如 3)桶数会变成 0 或 1,
 *      候选桶太少,插几个就开始疯狂踢出。
 *
 * 例:estimated_keys = 1000
 *      upper_pow_of_2(1000) = 1024
 *      1024 / 4             = 256 个桶 → 共 1024 个槽
 *
 * 例:estimated_keys = 10
 *      upper_pow_of_2(10) = 16,16/4 = 4 → 小于下限,取 16 个桶
 *
 * 注意这只是"起步值"。装不下时靠 cuckoo_filter_expand 追加更大的表,
 * 所以这个数估小了不会出错,只是会更早触发扩容。
 * -------------------------------------------------------------------------- */
static size_t cuckoo_estimate_buckets(size_t estimated_keys) {
    size_t nbuckets = upper_pow_of_2(estimated_keys)/CUCKOO_FILTER_TAGS_PER_BUCKET; //2^n/4
    return nbuckets < CUCKOO_FILTER_TABLE_MIN_BUCKETS ? CUCKOO_FILTER_TABLE_MIN_BUCKETS : nbuckets; //[16,2^n/4]
}

/* -----------------------------------------------------------------------------
 * 对外 API 用的是"枚举下标"(0/1/2/3),内部用的是"真实位宽"(8/12/16/32)。
 * 这张表负责两者之间的翻译。
 *
 *   下标 0 = CUCKOO_FILTER_BITS_PER_TAG_8   → 8  位
 *   下标 1 = CUCKOO_FILTER_BITS_PER_TAG_12  → 12 位
 *   下标 2 = CUCKOO_FILTER_BITS_PER_TAG_16  → 16 位
 *   下标 3 = CUCKOO_FILTER_BITS_PER_TAG_32  → 32 位
 *
 * 位宽怎么选?位宽 = 假阳性率与内存的权衡:
 *   8  位 → FPP ≈ 1/256   ≈ 0.4%,每 key 1 字节
 *   12 位 → FPP ≈ 1/4096  ≈ 0.02%
 *   16 位 → FPP ≈ 1/65536 ≈ 0.0015%
 *   32 位 → 基本不会误判,但每 key 4 字节,失去了"省内存"的意义
 * -------------------------------------------------------------------------- */
static int bits_per_tag_array[CUCKOO_FILTER_BITS_PER_TAG_TYPES] = {8,12,16,32};

/* 枚举下标 → 真实位宽。assert 只在 debug 构建生效,用来挡住越界下标。 */
static inline int cuckoo_get_bits_per_tag(int bits_per_tag_type) {
    assert(bits_per_tag_type < CUCKOO_FILTER_BITS_PER_TAG_TYPES); //4种类型
    return bits_per_tag_array[bits_per_tag_type];
}

/* -----------------------------------------------------------------------------
 * 初始化一张 cuckoo 表:算好各种尺寸,然后申请那块大数组。
 *
 * 参数:
 *   bits_per_tag — 真实位宽 8/12/16/32(不是枚举下标!)
 *   nbuckets     — 桶数,必须是 2 的幂(否则 & (n-1) 取模不成立,assert 会拦住)
 *
 * bytes_per_bucket 怎么来的:
 *   一个桶 = 4 个槽 × bits_per_tag 位,换成字节要向上取整。
 *   (bits*4 + 7) >> 3 就是 "ceil(bits*4 / 8)" 的常见写法:
 *   先加 7 保证有余数时能进位,再右移 3 位等价于除以 8。
 *
 *     8 位 → (8*4+7)>>3  = 39>>3  = 4  字节/桶   每槽正好 1 字节
 *    12 位 → (12*4+7)>>3 = 55>>3  = 6  字节/桶   4 个 12 位紧凑挤在 48 位里
 *    16 位 → (16*4+7)>>3 = 71>>3  = 8  字节/桶   每槽 2 字节
 *    32 位 → (32*4+7)>>3 = 135>>3 = 16 字节/桶   每槽 4 字节
 *
 * 为什么用 zcalloc 而不是 zmalloc:
 *   zcalloc 会把内存清零,而 tag == 0 就是"空槽"的约定,
 *   所以分配完不需要再遍历一遍把每个槽标记为空,省一次全量写。
 *
 * victim 三个字段全部归零 = "当前没有流浪的 tag"。
 * -------------------------------------------------------------------------- */
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



/* -----------------------------------------------------------------------------
 * siphash-2-4 需要的 128 位(16 字节)密钥。
 *
 * 这里是硬编码的固定值,不是随机生成的。含义:
 *   ✓ 好处:同一个 key 在任何进程、任何机器上算出同样的哈希,行为可复现,
 *          方便测试和排查问题。
 *   ✗ 风险:密钥公开 → 攻击者可以离线构造大量"哈希值相同"的 key,
 *          故意把它们塞进同一对桶,把过滤器打满、逼出大量误判
 *          (这类攻击叫 hash flooding)。
 *
 * 如果 key 来自不可信的外部输入(比如用户上传的字符串),
 * 应该改成进程启动时用随机数填充这 16 字节。
 * -------------------------------------------------------------------------- */
static uint8_t cuckoo_hash_function_seed[16] =
{15, 228, 29, 66, 3, 163, 118, 182, 101, 208, 229, 232, 2, 74, 115, 47};

/* siphash 的实现在 ../siphash/siphash.c,这里只做前置声明,避免多引一个头文件。 */
uint64_t siphash(const uint8_t *in, const size_t inlen, const uint8_t *k);

/* -----------------------------------------------------------------------------
 * 默认哈希函数:siphash-2-4,输出 64 位。
 * 调用 cuckoo_filter_new 时可以直接把这个函数传进去当 hash_fn。
 *
 * 为什么选 siphash 而不是更快的 xxhash/murmur:
 *   siphash 是"带密钥的伪随机函数",抗碰撞构造,专门为哈希表防攻击设计,
 *   同时速度也够用(Redis 的 dict 用的就是它)。
 * -------------------------------------------------------------------------- */
uint64_t cuckoo_gen_hash_function(const void *key, int len) {
    return siphash(key,len,cuckoo_hash_function_seed);
}

/* -----------------------------------------------------------------------------
 * 检测当前机器是不是小端(little endian)。
 *
 * 做法:造一个 int n = 1,内存里是 4 字节。
 *      小端把最低有效字节放在最前面 → 首字节是 0x01。
 *      大端反过来 → 首字节是 0x00。
 *      所以把 &n 强转成 char* 读第一个字节,是 1 就是小端。
 *
 *      内存低地址 →→→ 高地址
 *      小端: 01 00 00 00      *(char*)&n == 1
 *      大端: 00 00 00 01      *(char*)&n == 0
 *
 * 为什么要检查:12-bit tag 的读写(见 read_tag / write_tag)是直接把
 * 内存当 uint16 来移位取半个字节的,这套位运算依赖字节序。
 * 大端机器上会读到错位的数据,所以在 filter_new 里直接 assert 拦死。
 * -------------------------------------------------------------------------- */
static int is_littlen_endian() { int n = 1; return (*(char *)&n == 1); }

/* -----------------------------------------------------------------------------
 * 创建一个 cuckoo filter(对外入口)。
 *
 * 三个参数:
 *   hash_fn           — 哈希函数,一般直接传 cuckoo_gen_hash_function
 *   bits_per_tag_type — 枚举下标 CUCKOO_FILTER_BITS_PER_TAG_8/12/16/32
 *   estimated_keys    — 你预计要装多少 key,用来决定第一张表多大
 *
 * 步骤:
 *   1. assert 机器是小端(12-bit 打包的前提)。
 *   2. estimated_keys → 首表桶数。
 *   3. 分配 filter 结构体。
 *   4. 分配 1 张表的数组,并 init(里面会 zcalloc 出真正的数据区)。
 *
 * 初始状态:ntables = 1。之后不够用了由 cuckoo_filter_expand 往后追加。
 *
 * 用法示例:
 *   // 预计 100 万 key,8-bit 指纹(约 0.4% 误判率)
 *   cuckoo_filter_t *f = cuckoo_filter_new(cuckoo_gen_hash_function,
 *                                          CUCKOO_FILTER_BITS_PER_TAG_8,
 *                                          1000000);
 *   cuckoo_filter_insert(f, "user:1", 6);
 *   if (cuckoo_filter_contains(f, "user:1", 6) == CUCKOO_OK) { ... }
 *   cuckoo_filter_free(f);
 * -------------------------------------------------------------------------- */
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

/* -----------------------------------------------------------------------------
 * 释放单张表的数据区。
 * 释放后把指针置 NULL,这样重复调用不会 double free(幂等)。
 * 注意:只释放 data,不释放 table 结构体本身 —— 结构体是表数组的一个元素,
 *      由 cuckoo_filter_free 统一 zfree(filter->tables)。
 * -------------------------------------------------------------------------- */
void cuckoo_table_deinit(cuckoo_table_t *table) {
    if (table->data) {
        zfree(table->data);
        table->data = NULL;
    }
}

/* -----------------------------------------------------------------------------
 * 释放整个 filter。三层内存要按顺序拆:
 *   1. 每张表的 data(大数组)
 *   2. tables 数组本身
 *   3. filter 结构体
 * 传 NULL 是安全的(直接返回)。调用之后这个指针就悬空了,不要再用。
 * -------------------------------------------------------------------------- */
void cuckoo_filter_free(cuckoo_filter_t* filter) {
    if (filter == NULL) return;
    for (int i = 0; i < filter->ntables; i++) {
        cuckoo_table_deinit(filter->tables + i);
    }
    zfree(filter->tables);
    zfree(filter);
}

/* 小包装:调用用户注册的哈希函数。抽出来是为了让上层调用点更短、更好读。 */
static inline uint64_t cuckoo_filter_generate_hash(cuckoo_filter_t *filter,
        const char *key, size_t klen) {
    return filter->hash_fn(key,klen);
}

/* =============================================================================
 * 核心算法 ① 哈希切分:一个 hv 榨出"桶号"和"指纹"
 * =============================================================================
 *
 * 输入一个 64 位哈希 hv,输出两样东西:
 *   *i1  = 主候选桶号,范围 [0, nbuckets)
 *   *tag = 指纹,占 bits_per_tag 位,且保证 != 0
 *
 * 为什么只哈希一次就够?
 *   因为 64 位里信息量足够多。高 32 位和低 32 位在一个好的哈希函数里
 *   是相互独立的,所以可以当成"两个独立哈希"来用,省掉一次哈希开销。
 *
 * 逐行拆解(假设 nbuckets = 2^23 = 8M,bits_per_tag = 8):
 *
 *   ① *i1 = (hv >> 32) & (nbuckets - 1)
 *      hv >> 32          取出高 32 位
 *      & (8M - 1)        等价于 % 8M,即只保留最低 23 位
 *      为什么能用 & 代替 %:因为 nbuckets 是 2 的幂,
 *      8M-1 = 0x7FFFFF = 二进制 23 个 1,相与就是"截断到 23 位"。
 *
 *   ② *tag = (hv & 0xFFFFFFFF) & ((1ULL << bits_per_tag) - 1)
 *      hv & 0xFFFFFFFF   取出低 32 位(这一步其实是为了表达意图,
 *                        后面的掩码已经把范围收得更窄了)
 *      & ((1<<8) - 1)    保留最低 8 位 → 得到 0..255
 *
 *   ③ *tag += (*tag == 0)
 *      这行是个技巧,等价于:if (*tag == 0) *tag = 1;
 *      因为 C 里比较表达式的结果是 0 或 1,直接加上去就行,还能避免分支。
 *      为什么必须这么做:tag == 0 被约定为"空槽",
 *      如果真实 tag 允许为 0,那么"某个 key 存进去了"和"这个槽没用过"
 *      就无法区分,查询会出现 false negative。
 *      副作用:tag 为 1 的概率是其他值的两倍(0 和 1 都映射到 1),
 *      误判率因此略微升高一点点,可以忽略。
 * -------------------------------------------------------------------------- */
static inline void cuckoo_table_index_tag(cuckoo_table_t* table, uint64_t hv,
        size_t *i1, uint32_t *tag) {
    *i1 = (hv >> 32) & (table->nbuckets -1); // 取高 32 位  & (2^n - 1) =>  (hv >> 32) & (8M (2^23) -1)  (取后23位)
    *tag = (hv & 0xFFFFFFFF) & ((1ULL << table->bits_per_tag) - 1); // 取低 32位 & （1 << 8-1 )。（取后8位）
    *tag += *tag == 0;
}

/* =============================================================================
 * 核心算法 ② Alternate Index:算出"另一个家"在哪
 * =============================================================================
 *
 * 公式:
 *     i2 = (i1 XOR (tag * 0x5bd1e995)) & (nbuckets - 1)
 *
 * 这是整个 cuckoo filter 最精妙的一步,它有一个必需的性质 —— 对称性:
 *
 *     alt(i1, tag) == i2      并且      alt(i2, tag) == i1
 *
 * 为什么成立?令 h = tag * 0x5bd1e995(对同一个 tag 是固定值),则
 *     i2 = i1 ^ h
 *     alt(i2, tag) = i2 ^ h = (i1 ^ h) ^ h = i1        (XOR 自反:x^h^h == x)
 *
 * 这个性质为什么关键?
 *   踢出链条里,我们手上只有"一个流浪的 tag"和"它刚被踢出的桶号",
 *   并没有原始的 key(key 早就不存了!)。靠对称性,我们仍然能唯一算出
 *   "这个 tag 的另一个合法位置在哪",链条才能继续走下去。
 *   查询时也一样:只要有 i1 和 tag,就能推出 i2,不需要第二次哈希。
 *
 * 常数 0x5bd1e995 的来历和要求:
 *   - 来自 MurmurHash2 的乘子,业界公认混合效果好。
 *   - 必须是奇数:偶数会让低位固定为 0,乘完之后桶号分布出现空洞。
 *   - 二进制 0101 1011 1101 0001 1110 1001 1001 0101,0/1 分布均匀,
 *     乘法能把 tag 那几位的变化"雪崩"扩散到整个 32 位。
 *   - 如果这里图省事写成 i2 = i1 ^ tag,那么 i1 和 i2 只在 tag 的低几位上
 *     不同 → 两个候选桶挨得很近甚至落在同一 cache line 的邻居,
 *     分布严重不均,负载率上不去。
 *
 * 一个小陷阱:如果 h & (nbuckets-1) 恰好为 0,则 i2 == i1,
 * 该 key 就只有 4 个槽可用而不是 8 个。概率是 1/nbuckets,影响可忽略。
 * -------------------------------------------------------------------------- */
static inline size_t cuckoo_table_alt_index(cuckoo_table_t* table, size_t i1, uint32_t tag) {
    return (i1 ^ ((size_t) tag * 0x5bd1e995)) & (table->nbuckets - 1);
}

/* =============================================================================
 * 核心算法 ③a 读取:把 bucket i 的第 j 个槽里的 tag 取出来
 * =============================================================================
 *
 * 先定位到桶的起始字节:
 *     p = data + i * bytes_per_bucket
 *
 * 然后按位宽分四种情况。8/16/32 位都能被字节整除,所以直接把 p 转成
 * 对应宽度的数组下标访问就行,编译器生成一条 load 指令:
 *
 *     8  位:((uint8_t*) p)[j]
 *     16 位:((uint16_t*)p)[j]
 *     32 位:((uint32_t*)p)[j]
 *
 * 12 位是唯一麻烦的,因为它不是整字节。4 个槽 × 12 位 = 48 位 = 6 字节,
 * 槽和字节边界"错位"排列(下图每格一个字节,数字表示归属哪个槽):
 *
 *     字节:   0        1        2        3        4        5
 *           ┌────────┬────────┬────────┬────────┬────────┬────────┐
 *           │  tag0  │t0 │ t1 │  tag1  │  tag2  │t2 │ t3 │  tag3  │
 *           │  低8位 │高4│低4 │  高8位 │  低8位 │高4│低4 │  高8位 │
 *           └────────┴────────┴────────┴────────┴────────┴────────┘
 *     位:   0      7 8     15 16    23 24    31 32    39 40    47
 *
 * 观察规律:偶数槽(j=0,2)从某个字节的最低位开始,占该处 16 位里的低 12 位;
 *          奇数槽(j=1,3)则占某个 16 位里的高 12 位。
 *
 * 代码怎么算的:
 *   p += j + (j >> 1);           // 等价于 p += floor(1.5 * j),定位到起始字节
 *                                //   j=0 → +0    j=1 → +1
 *                                //   j=2 → +3    j=3 → +4
 *   uint16_t w = *(uint16_t*)p;  // 一次读 16 位,目标 12 位一定在里面
 *   w >>= (j & 1) << 2;          // 奇数槽右移 4 位((j&1)*4),偶数槽不移
 *   tag = w & 0xFFF;             // 保留低 12 位
 *
 * 手算 j = 3:p += 3 + 1 = 4,读字节 4~5(即位 32~47),
 *            右移 4 → 位 36~47,& 0xFFF → 正好是 tag3。✓
 *
 * 小端依赖在哪:`*(uint16_t*)p` 假定"低地址字节 = 低有效位"。
 *              大端机器上这两个字节会被解释成反的,取出来是垃圾。
 *
 * 越界安全:j=3 时读字节 4~5,桶一共 6 字节(下标 0~5),刚好不越界。
 * -------------------------------------------------------------------------- */
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

/* =============================================================================
 * 核心算法 ③b 写入:把 tag 放进 bucket i 的第 j 个槽
 * =============================================================================
 *
 * 和 read_tag 完全对称。8/16/32 位直接下标赋值。
 *
 * 12 位要走"先清空、再或入"两步,因为目标 12 位和邻居槽共享同一个字节,
 * 不能整体覆盖,否则会把隔壁的 tag 抹掉:
 *
 *   偶数槽 (j & 1 == 0):目标在 16 位的低 12 位
 *       w &= 0xF000;      // 清掉低 12 位,保住高 4 位(那是邻居槽的一部分)
 *       w |= tag;         // 写入
 *
 *   奇数槽 (j & 1 == 1):目标在 16 位的高 12 位
 *       w &= 0x000F;      // 清掉高 12 位,保住低 4 位(邻居的)
 *       w |= tag << 4;    // 左移到高 12 位再写入
 *
 * 图示写 tag1(奇数槽,占字节 1 的高 4 位 + 字节 2 的全部):
 *
 *   原始 16 位(p 指向字节 1):  [ t1高4 | t0高4 ]  ← 低 4 位属于 tag0
 *   w &= 0x000F              →  [  0000 0000 0000 | t0高4 ]
 *   w |= tag << 4            →  [   新 tag1 的 12 位   | t0高4 ]   ✓ tag0 完好
 *
 * 参数类型是 int32_t 而不是 uint32_t,写入时被隐式截断到目标宽度,
 * 传 CUCKOO_TAG_NULL (0) 就相当于"清空这个槽"(delete 就是这么用的)。
 *
 * 前提:传进来的 tag 不会是 0(index_tag 里已经 +1 保证过),
 *      所以写完再读一定 != CUCKOO_TAG_NULL —— 除了 delete 故意写 0 的情况。
 * -------------------------------------------------------------------------- */
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

/* =============================================================================
 * 核心算法 ④ 温和插入:只找现成空槽,不踢任何人
 * =============================================================================
 *
 * 这是插入的"第一次尝试",最快的路径。
 *
 * 流程:
 *   1. hv → (i1, tag),再算 i2 = alt(i1, tag)
 *   2. 顺序扫 i1 的 4 个槽,遇到第一个空槽(tag == 0)就写进去,ntags++,成功
 *   3. i1 全满,再扫 i2 的 4 个槽,同样处理
 *   4. 8 个槽都满 → 返回 CUCKOO_ERR,交给调用方决定"踢出"还是"扩容"
 *
 *     i1 [3A][00][7F][00]     ← 扫到第 1 个 00,写在这里,结束
 *     i2 [C1][C1][09][2B]     ← 这次用不上
 *
 * 时间复杂度:最多 8 次 read_tag,而 i1、i2 各自的 4 个槽都在同一个桶里
 *            (连续内存),所以实际上只有 2 次 cache miss。这就是
 *            cuckoo filter 快的原因。
 *
 * 一个重要的语义:本函数不检查"这个 tag 是不是已经在里面了"。
 *   → 同一个 key 插 N 次,会占用 N 个槽。
 *   → 好处:插入路径没有额外比较,永远 O(1),也不需要额外的去重结构。
 *   → 坏处:重复 key 浪费空间,ntags 会虚高,负载率看起来比实际紧张。
 *   → 对误判率没有影响(重复的是同一个指纹)。
 *   如果业务上不能容忍重复,应该在调用 insert 前先 contains 一次。
 * -------------------------------------------------------------------------- */
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

/* -----------------------------------------------------------------------------
 * 取"当前表" = 数组最后一张 = 最新、最大的那张。
 * filter 至少持有 1 张表(filter_new 里就建了),所以 ntables-1 >= 0,指针必有效。
 * 插入新数据、扩容都以这张表为主战场。
 * -------------------------------------------------------------------------- */
static inline cuckoo_table_t* cuckoo_filter_current_table(cuckoo_filter_t* filter) {
    return &filter->tables[filter->ntables-1];
}

/* =============================================================================
 * 核心算法 ⑤ 踢出插入:布谷鸟本人登场
 * =============================================================================
 *
 * 什么时候走到这里:no_kick 失败(8 个候选槽全满)。
 *
 * 注意函数签名收的是 (i, tag) 而不是 hv —— 因为踢出链条中途手上只有
 * "流浪的 tag + 它该去的桶号",原始 key/hv 早就没了。
 *
 * 【第 0 步:进门先看 victim 是否已被占用】
 *
 *     if (table->victim.used) return CUCKOO_ERR;
 *
 * 这一行是整个"表满"判定的关键闸门,含义是:
 *   这张表上次已经有一个 tag 踢不动、寄存在 victim 里了。
 *   victim 只有一个格子,现在再来一个我没地方放,所以干脆不接这活,
 *   直接报错让上层(cuckoo_filter_insert)去 expand 开新表。
 *
 * 为什么必须这么做:如果不检查就往下走,500 轮之后会把新 tag 写进 victim,
 * 把上一个 tag 直接盖掉 —— 那个 tag 就彻底消失了,contains 会返回"不存在",
 * 这是 false negative,直接违背 cuckoo filter 的核心承诺。宁可拒绝插入,
 * 也不能悄悄丢数据。
 *
 * 代价:判定偏保守。哪怕当前桶 i 其实还有空槽,只要 victim 被占着也一律拒绝。
 *      不过这不会让表变成"死表" —— no_kick 路径不受影响,
 *      而且一旦发生一次 delete,try_eliminate_victim_cache 会把 victim 腾空,
 *      这张表又能重新接受踢出插入。
 *
 * 由此得到一条清晰的语义:一张表被认为"满了",指的是
 *   (a) 目标 key 的 8 个候选槽全满,并且 (b) victim 格子也已经被占。
 * 换句话说,每张表允许发生一次"踢不动"事件,第二次就触发扩容。
 *
 * 每一轮外层循环做两件事:
 *   ① 扫当前桶 i 的 4 个槽,有空位就写进去,ntags++,成功返回。
 *      (注意:虽然进来时是"满"的,但踢出链条走到下一个桶时,那个桶很可能有空位,
 *       所以绝大多数情况在第 1~2 轮就结束了。)
 *   ② 全满 → 随机挑一个槽 kj,把它的老 tag 读出来叫 otag,
 *      新 tag 写进去。然后 tag = otag(换成"现在流浪的是 otag"),
 *      i = alt(i, otag)(otag 的另一个家),继续下一轮。
 *
 * 一轮迭代的图解:
 *
 *   进来:i=5, tag=X,桶 5 满了
 *     bucket5 [A][B][C][D]
 *              ↓ 随机选中 kj=1
 *     otag = B,写入 X
 *     bucket5 [A][X][C][D]      ← X 安家了
 *     下一轮:tag=B, i=alt(5,B)=17 → 去桶 17 试
 *
 * 为什么用 rand() 随机选槽?
 *   如果固定选第 0 个槽,某些 key 组合会形成稳定的踢出环
 *   (A 踢 B、B 踢 A、无限循环)。随机化打破这种周期性,
 *   让链条有机会跳出去找到空位。这是论文里推荐的做法。
 *
 * 循环上限 CUCKOO_FILTER_MAX_ITERATION (500):
 *   踢了 500 次还没安置下来,说明表基本满了(负载率通常已 > 95%),
 *   或者运气极差撞进了长环。这时不能无限循环,必须收尾。
 *
 * 收尾 = 写入 victim cache:
 *   把最后手上那个 (i, tag) 存到表的 victim 槽里,ntags++,仍然返回 CUCKOO_OK。
 *   为什么不能直接丢弃?丢了就等于"插进去的东西查不到了",
 *   破坏了 cuckoo filter "绝不产生 false negative" 的核心保证。
 *   contains / delete 都会额外检查 victim,所以它仍然是可查、可删的。
 *   走到这里时 victim 一定是空的(第 0 步已经保证),所以不存在覆盖问题。
 *
 * 三种返回情况总结:
 *   CUCKOO_OK  + 写进了某个桶的空槽     ← 绝大多数情况,链条中途就安置好了
 *   CUCKOO_OK  + 寄存在 victim 里       ← 500 轮耗尽,但 victim 是空的,兜住了
 *   CUCKOO_ERR + 什么都没做             ← victim 已被占用,这张表放不下了,
 *                                         上层应该 expand
 *
 * ⚠️ 调用方注意:返回 CUCKOO_ERR 时,tag 完全没有被写入任何地方,
 *   调用方有责任继续安置它(expand 后重试),否则这个 key 就丢了。
 *   目前只有 cuckoo_filter_insert 正确处理了这一点;
 *   cuckoo_table_try_eliminate_victim_cache 忽略了返回值,
 *   但它调用前刚把 victim 清空,所以不可能撞上这个分支(见那里的注释)。
 * -------------------------------------------------------------------------- */
int cuckoo_table_insert_kick_out_index_tag(cuckoo_table_t* table, size_t i,
        uint32_t tag) {
    uint32_t otag;

    if (table->victim.used) return CUCKOO_ERR;
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

/* -----------------------------------------------------------------------------
 * kick_out 的便捷入口:输入是 hv(有原始 key 的场景),
 * 先拆出 (i, tag),再交给上面的核心循环。
 *
 * 两个入口的区别:
 *   ..._kick_out()            ← 新 key 插入时用,从 hv 起步
 *   ..._kick_out_index_tag()  ← 踢出链条中途、或 victim 重新安置时用,
 *                               直接从 (i, tag) 起步
 *
 * 返回值原样透传,所以这里也可能返回 CUCKOO_ERR(末表 victim 已占用)。
 * 调用方 cuckoo_filter_insert 会把它当作"该扩容了"的信号。
 * -------------------------------------------------------------------------- */
int cuckoo_table_insert_kick_out(cuckoo_table_t* table, uint64_t hv) {
    size_t i;
    uint32_t tag;
    cuckoo_table_index_tag(table, hv, &i, &tag);
    return cuckoo_table_insert_kick_out_index_tag(table, i, tag);
}

/* -----------------------------------------------------------------------------
 * 从 (i, tag) 起步的完整插入:先扫两个候选桶找现成空槽,都满了才启动踢出链。
 *
 * 和已有的两个入口的分工:
 *   insert_no_kick(hv)            ← 有 hv,只找空槽
 *   insert_kick_out_index_tag(i,t) ← 只有 (i,tag),直接踢
 *   insert_index_tag(i,t)         ← 只有 (i,tag),但也想先享受一下空槽快路径
 *
 * 为什么需要它:缩容迁移时手上只有 (旧桶号, tag),没有 key 也没有 hv,
 * 用不了 no_kick;而直接上 kick_out 只扫 i 一个桶就开始踢,
 * 会把本来能平放进 alt(i) 的 tag 也搅进踢出链,白白抬高迁移成本和失败率。
 *
 * 语义与返回值和 insert_kick_out_index_tag 完全一致(含"victim 已占用则 ERR"),
 * 只是在前面加了 8 个槽的空位探测。
 * -------------------------------------------------------------------------- */
int cuckoo_table_insert_index_tag(cuckoo_table_t* table, size_t i, uint32_t tag) {
    size_t i2 = cuckoo_table_alt_index(table, i, tag);

    for (int j = 0; j < CUCKOO_FILTER_TAGS_PER_BUCKET; j++) {
        if (cuckoo_table_read_tag(table, i, j) == CUCKOO_TAG_NULL) {
            cuckoo_table_write_tag(table, i, j, tag);
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

    return cuckoo_table_insert_kick_out_index_tag(table, i, tag);
}

/* =============================================================================
 * 核心算法 ⑦ 阶梯扩容:不搬数据,直接加一张更大的表
 * =============================================================================
 *
 * 传统 cuckoo filter 扩容要"新建大表 + 把所有 tag 重新哈希搬过去",
 * 问题是:tag 只是指纹,原始 key 已经不存在了,没法重新计算新表的桶号!
 * 所以经典实现要么不支持扩容,要么必须保留 key。
 *
 * 本实现绕开这个难题:旧表原封不动,新增一张 4 倍大的表接收新数据。
 *
 *     扩容前: tables = [ 16桶 ]
 *     扩容后: tables = [ 16桶 ][ 64桶 ]
 *     再扩容: tables = [ 16桶 ][ 64桶 ][ 256桶 ]
 *                                          ↑ 新数据都往最后这张放
 *
 * 步骤:
 *   1. 拿末表的 nbuckets,乘 CUCKOO_FILTER_BUCKETS_EXPANSION (4)。
 *   2. 先检查表数是否已达 CUCKOO_FILTER_MAX_TABLES (4),满了直接 ERR。
 *   3. 桶数封顶在 UINT32_MAX(因为 index_tag 只用 hv 的高 32 位算桶号,
 *      桶数超过 2^32 的话高位就不够用了)。
 *   4. ntables++,zrealloc 把 tables 数组扩长一格(旧内容自动保留)。
 *   5. ⚠️ realloc 可能搬动整个数组的地址,所以必须重新取一次末表指针,
 *      不能沿用第 1 步那个 table 变量 —— 那是常见的 use-after-realloc bug。
 *   6. 对新末表 table_init,分配并清零它的数据区。
 *
 * 代价:
 *   - 内存:每次 ×4。首表 1 单位,连扩 3 次总共 1+4+16+64 = 85 倍。
 *   - 查询:表数每多一张,contains/delete 就多扫 2 个桶。最多 4 张 → 8 个桶。
 *   - 没有缩容:哪怕把所有 key 都删掉,内存也不会还给系统。
 *
 * 返回值注意:成功返回的是字面量 0(恰好等于 CUCKOO_OK),
 *            失败返回 CUCKOO_ERR。风格上不太统一,但行为正确。
 * -------------------------------------------------------------------------- */
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

/* =============================================================================
 * 核心算法 ⑧ 迁移原语:为什么"变小"可以做,而"变大"做不到
 * =============================================================================
 *
 * ## 不对称性从哪来
 *
 * 桶号是掩码截断出来的:
 *
 *     i1 = (hv >> 32) & (nbuckets - 1)
 *
 * 缩容到 N' = N / 2^k 时,新桶号就是旧桶号丢掉几个高位:
 *
 *     i1' = (hv >> 32) & (N' - 1) = i1 & (N' - 1)
 *
 * 右边只用到 i1,不需要 hv —— 所以光凭表里的信息就能算出来。
 * 反过来扩容需要 i1 里没有的更高位,而那些位在"只存指纹"的表里
 * 早就被丢掉了,任何算法都恢复不出来。这才是经典 cuckoo filter
 * "不支持 resize"的准确含义:它指的是不支持变大。
 * (本实现的 cuckoo_filter_expand 正是因此才选择"另开一张新表"。)
 *
 * ## 迁移时的一个坎:不知道 i 是 i1 还是 i2
 *
 * 遍历旧表时,我们看到的是"tag T 躺在桶 i 里",但 i 可能是 T 的主桶,
 * 也可能是备桶,表里没有记录。好在 alt() 的对称性把这个问题直接消掉了。
 * 记 h = T * 0x5bd1e995,M' = N' - 1:
 *
 *   若 i 是 i1: i1' = i & M'         i2' = (i1' ^ h) & M' = (i ^ h) & M'
 *   若 i 是 i2: i1' = (i ^ h) & M'   i2' = (i1' ^ h) & M' = i & M'
 *
 * 两种情况下候选桶**集合**都是 { i & M', (i ^ h) & M' },完全一致!
 * (中间用到 ((i ^ h) & (N-1)) & M' == (i ^ h) & M',因为 M' 的位是 N-1 的子集。)
 *
 * 所以迁移规则极其简单:**从 i & M' 起步插入新表**,不必关心 i 的角色。
 * 落点一定在该 tag 的合法候选桶里 → contains 照样找得到 →
 * "绝不产生 false negative"的保证原样传下去。
 *
 * ## 事务性:要么全部迁完,要么一个字节都不改
 *
 * 新表只有一个 victim 槽,tag 挤不进去时 insert 会返回 CUCKOO_ERR。
 * 一旦中途失败就必须放弃 —— 已经迁过去的那些 tag 连同新表一起丢掉,
 * 旧表全程只被读、没被写,所以天然回滚。
 * 绝对不能"迁一半就提交",那会静默丢 tag,直接制造 false negative。
 *
 * ## 代价
 *
 *   - 时间:O(源表桶数 × 4) 次读 + O(ntags) 次插入,是一次全量重建。
 *   - 峰值内存:重建期间新旧两份数据同时存在(新的只有旧的 1/4,可接受)。
 *   - 假阳性率:桶变少 → 负载率升高 → 桶更满 → 查询撞上匹配 tag 的概率上升。
 *     单表 FPP 的上界 1-(1-1/(2^f-1))^8 不变,但会更贴近这个上界。
 *     这是"省内存"换来的,没有免费午餐。
 *
 * ## 接口约定
 *
 * 唯一前提是 dst->nbuckets <= src->nbuckets —— 这就是"截断可算"的条件。
 * 允许相等:此时 mask 是恒等映射,候选桶集合和原来一模一样,等于原样重建一遍。
 * 合表就是用的相等这一档(临时表与前表等大),外加一次真正的 1/4 截断(末表)。
 *
 * 只写 dst,src 全程只读。这一点是事务性的基础:
 * 中途失败时 dst 已被部分写入,但只要调用方把 dst 整个丢掉,src 依然完好。
 * 所以本函数**不负责回滚**,由调用方保证 dst 是一张可以随时丢弃的临时表。
 * -------------------------------------------------------------------------- */
static int cuckoo_table_migrate_tags(cuckoo_table_t* dst, cuckoo_table_t* src) {
    assert(dst->nbuckets <= src->nbuckets);
    assert(dst->bits_per_tag == src->bits_per_tag);
    size_t mask = dst->nbuckets - 1;

    for (size_t i = 0; i < src->nbuckets; i++) {
        for (size_t j = 0; j < CUCKOO_FILTER_TAGS_PER_BUCKET; j++) {
            uint32_t tag = cuckoo_table_read_tag(src, i, j);
            if (tag == CUCKOO_TAG_NULL) continue;
            if (cuckoo_table_insert_index_tag(dst, i & mask, tag) != CUCKOO_OK)
                return CUCKOO_ERR;
        }
    }

    /* victim 里那个 tag 同样是一份真实数据,漏搬就是静默丢数据。
     * 它的 index 就是当初踢出链停下来的桶号,截断规则和普通槽完全一样。 */
    if (src->victim.used) {
        if (cuckoo_table_insert_index_tag(dst, src->victim.index & mask,
                    src->victim.tag) != CUCKOO_OK)
            return CUCKOO_ERR;
    }
    return CUCKOO_OK;
}

/* =============================================================================
 * 核心算法 ⑨ Filter 级缩容:摘掉末表,严格作为 expand 的逆操作
 * =============================================================================
 *
 * ## 为什么必须是"合并末表",而不是"每张表各自瘦身"
 *
 * expand 干的事是:在数组末尾追加一张 4 倍大的表,ntables++。
 * 那么缩容就必须是它的镜像:把末表的数据并回前一张,丢掉末表,ntables--。
 * 理由有三条,都是硬的:
 *
 *   1. 表数才是主导代价。contains / delete 是 O(ntables) 逐表遍历,
 *      假阳性率也随表数近似线性上升:
 *          FPP ≈ 1 - (1 - 1/(2^f - 1))^(8 × ntables)
 *      8-bit tag 实测:1 张表 3.0%,2 张 6.0%,3 张 8.9%,4 张 11.7%。
 *      "各表就地瘦身"完全不动 ntables,这两项一点都改善不了。
 *
 *   2. 阶梯必须守住。tables 的尺寸是 1:4:16:64 的等比阶梯,expand 直接拿
 *      末表尺寸 × 4 当新表尺寸。合并末表之后阶梯原样保持([16][64][256]),
 *      下一次 expand 会精确重建刚摘掉的那张表 —— 完美可逆。
 *      而各表就地瘦身会把阶梯打乱,让 expand 从一个奇怪的基数起算,
 *      连"总容量还剩多少"都变得没法推算。
 *
 *   3. 内存主要压在末表。1+4+16+64 里末表占 64/85 ≈ 75%,
 *      摘掉它一次就能回收大头。
 *
 * ## 方向为什么成立
 *
 * 末表 N_src 桶,前表 N_dst = N_src / 4 桶,满足 N_dst < N_src,
 * 所以截断可算(推导见 cuckoo_table_migrate_tags 的注释)。
 *
 * ⚠️ 注意这里和"把所有表合并成一张"是两回事。后者要求目标 ≤ **所有**源表里
 * 最小的那张(也就是 tables[0]),而 tables[0] 往往只有别人的 1/4、1/16、1/64,
 * 根本装不下 —— 那个方向确实不可行。但"末表并入前一张"只涉及相邻两张,
 * 目标恰好是源的 1/4,完全成立。
 *
 * ## 对查询没有影响
 *
 * tag 是被搬到**老表**里去的,而且落点仍是它自己的合法候选桶
 * (截断 + alt 对称性保证),所以 contains 照样找得到 —— 不会产生
 * false negative。副作用反而是好的:要扫的表少了一张,查询更快、FPP 更低。
 *
 * ## 没有策略参数:说缩就缩,缩不动就返回 CUCKOO_ERR
 *
 * 这里刻意不提供"目标负载率"之类的旋钮,理由是量出来的:
 *
 *   1. 桶数必须是 2 的幂,所以这种阈值只有约 1 bit 的有效分辨率。
 *      实测把阈值从 0.5 一路调到 0.97,选出来的桶数完全一样(都是 256 桶),
 *      调它没有意义。
 *   2. 真正起作用的一直是硬容量上限(槽位数装不下)。软阈值那一道预检
 *      在真实负载下几乎从不生效 —— 实测 0/38 次通过。
 *   3. 阈值设错会直接引发抖动(缩完立刻被 expand 顶回去),是个陷阱。
 *   4. expand 本身也没有任何参数,保持对称。
 *
 * 所以只留一道 O(1) 的硬预检(装不下就不试),然后直接干。
 * 代价是数据较密时可能白做一次 O(桶数) 的迁移才发现放不下,
 * 然后回滚返回 CUCKOO_ERR。换来的是一个没有陷阱的接口。
 *
 * ## 一次调用只处理一张表
 *
 * 每次调用的工作量有上界(O(末表桶数)),延迟可预期,
 * 不会因为表多就一口气卡住调用线程。想缩到底就循环调用:
 *
 *     while (cuckoo_filter_shrink(filter) == CUCKOO_OK) ;
 *
 * 两条互斥的分支,都只在 ntables > 1 时成立,按顺序判定:
 *
 *   分支 A  末表为空(ntags == 0 且 victim 未占用)
 *           → 直接摘掉,零迁移成本,必定成功。最便宜,所以先判。
 *           这一条在实践中是主力:delete 是新→旧遍历、第一张命中就返回,
 *           所以删除会优先抽干**新**表(实测删掉 80% 后 tables[0] 仍是
 *           0.99 负载,而末表已经空了)。而末表恰好是最大的那张
 *           (1+4+16+64 里占 64/85 ≈ 75%),所以最贵的内存最容易被回收。
 *
 *   分支 B  末表非空
 *           → 把末表并入前一张。做法是新建一张与前表等大的临时表,
 *             先搬前表、再搬末表,两步都成功才提交。
 *             为什么不直接往前表里插?因为前表已经有数据,插一半失败
 *             就没法回滚了。用临时表才能做到"要么全成,要么两张原表都没动"。
 *             注意前表只有末表 1/4 的槽位,所以合并后负载率会放大到
 *             (1 + CUCKOO_FILTER_BUCKETS_EXPANSION) = 5 倍(实测比值精确为 5)。
 *             这条分支因此只在数据相当稀疏时才成立。
 *
 * ## ⚠️ 绝不缩基表(ntables == 1 时直接返回 CUCKOO_ERR)
 *
 * tables[0] 是"基表",本函数不碰它,理由有三条:
 *
 *   1. 它的尺寸是调用方通过 estimated_keys 声明的容量意图。
 *      偷偷把它改小等于覆盖调用方的决定。
 *   2. 它是阶梯的基准。expand 用"末表尺寸 × 4"算新表大小,
 *      所以基表尺寸决定了整条阶梯 nb0, 4·nb0, 16·nb0, 64·nb0。
 *      一旦把基表从 32768 缩成 512,下一次 expand 造出来的是 2048 桶
 *      而不是原来的 131072 —— 可逆性彻底丢失,而可逆性正是"合并末表"
 *      这个设计的全部立足点。同时 4 张表的总容量上限也被永久压低了。
 *   3. 缩基表根本不减少 ntables,对查询开销 O(ntables) 和
 *      FPP 的表数因子毫无贡献 —— 恰好丢掉了合表设计的全部收益。
 *
 * 所以本函数的语义收窄成一句话:**只摘表,不改基表尺寸。**
 * 真想回收基表那部分内存,只有重建 filter 一条路(用更小的 estimated_keys
 * 新建一个,把还活着的 key 重新插进去),那是调用方的决策,不该由缩容偷偷做。
 *
 * ## 语义保证
 *
 *   - 不丢 tag:两个分支都是事务性的,失败就整体回滚,原状态完好。
 *     所以调用前后都不会产生 false negative。
 *   - 基表始终不变:tables[0] 的 nbuckets 永远等于 cuckoo_filter_new
 *     当初算出来的值。filter 也因此始终持有 current_table,
 *     insert / expand 永远有落脚点。
 *   - 假阳性率:摘掉一张表会让 FPP 的 ntables 因子下降,
 *     但目标表负载率会上升。两个方向相反,净效果取决于数据稀疏程度。
 *     内存和 FPP 之间没有免费午餐 —— 归根结底 FPP 由"每个 key 摊到几个
 *     bit"决定,内存降下来 FPP 就会升。
 *   - 只搬运,不做语义清理:既不去重,也不会清掉历史误删留下的陈旧 tag。
 *
 * 返回:
 *   CUCKOO_OK  — 摘掉了一张表。
 *   CUCKOO_ERR — 缩不动(只剩基表 / 装不下 / 迁移失败),filter 一个字节都没变。
 *                可作为循环终止条件。
 * -------------------------------------------------------------------------- */
 /* 可优化成不申请中间table */
int cuckoo_filter_shrink(cuckoo_filter_t* filter) {
    if (filter == NULL) return CUCKOO_ERR;

    /* 只剩基表了,到底了。基表的尺寸是调用方通过 estimated_keys 声明的容量,
     * 同时又是 expand 计算新表尺寸的基数,不能碰。详见函数头注释。 */
    if (filter->ntables == 1) return CUCKOO_ERR;

    cuckoo_table_t* src = filter->tables + filter->ntables - 1;  /* 末表 */
    cuckoo_table_t* dst = filter->tables + filter->ntables - 2;  /* 前一张 */

    /* --- 分支 A:末表本来就是空的,直接摘,不用迁移 --- */
    if (src->ntags == 0 && src->victim.used == 0) {
        cuckoo_table_deinit(src);
        filter->ntables--;
        filter->tables = zrealloc(filter->tables,
                filter->ntables * sizeof(cuckoo_table_t));
        return CUCKOO_OK;
    }

    /* --- 分支 B:把末表并入前一张 --- */

    /* 唯一的预检,O(1):前表的槽位数能不能装下两张表的 tag 总和。
     * 这不是策略阈值,而是"物理上放不下"的判断,不装这一道就是白跑一趟迁移。 */
    size_t merged_ntags = dst->ntags + src->ntags;
    size_t dst_slots = dst->nbuckets * CUCKOO_FILTER_TAGS_PER_BUCKET;
    if (merged_ntags > dst_slots) return CUCKOO_ERR;

    /* 建一张与前表等大的临时表,两张原表都只读地搬进去。
     * 中途任何一步失败,丢掉临时表即可,两张原表一个字节都没动。 */
    cuckoo_table_t merged;
    cuckoo_table_init(&merged, dst->bits_per_tag, dst->nbuckets);
    if (cuckoo_table_migrate_tags(&merged, dst) != CUCKOO_OK ||
        cuckoo_table_migrate_tags(&merged, src) != CUCKOO_OK) {
        cuckoo_table_deinit(&merged);
        return CUCKOO_ERR;
    }

    /* 自检:两张表的 tag 一个不多一个不少地都搬过来了。 */
    assert(merged.ntags == merged_ntags);

    /* 提交:释放两张原表的数据区,前表位置换成合并结果,数组缩短一格。 */
    cuckoo_table_deinit(dst);
    cuckoo_table_deinit(src);
    *dst = merged;
    filter->ntables--;
    filter->tables = zrealloc(filter->tables,
            filter->ntables * sizeof(cuckoo_table_t));

    return CUCKOO_OK;
}

/* =============================================================================
 * 核心算法 ⑥ Filter 级插入:把上面那些零件串成完整流程(对外 API)
 * =============================================================================
 *
 * 输入 key + 长度,返回 CUCKOO_OK / CUCKOO_ERR。
 *
 * 四级降级策略(前面能成就不走后面,越往后越贵):
 *
 *   第 0 步  算一次 hv。注意:所有表共用同一个 hv,
 *            但因为每张表的 nbuckets 不同,& (nbuckets-1) 的结果不同,
 *            所以同一个 key 在不同表里落在不同的桶。
 *
 *   第 1 步  从末表往前(新 → 旧),对每张表试 no_kick。
 *            任意一张表有空槽就写进去,立即返回 OK。最快路径。
 *            为什么先试新表?新表大、空,命中空槽概率最高。
 *            为什么也试旧表?旧表可能因为 delete 腾出了空槽,能捡便宜,
 *            避免过早扩容。
 *
 *   第 2 步  所有表都塞不下 → 在末表启动 kick_out 踢出链。
 *            两种结果算成功:链条中途找到空槽,或者耗尽 500 轮后寄存进
 *            末表的 victim 格子(前提是那个格子还空着)。
 *
 *   第 3 步  kick_out 返回 ERR(说明末表的 victim 已经被占,这张表真的满了)
 *            → expand 追加一张 4 倍大的新表,在新表上 no_kick。
 *            新表是全空的,所以这一步必然成功。
 *
 *   第 4 步  expand 也失败(表数已达 CUCKOO_FILTER_MAX_TABLES)→ CUCKOO_ERR。
 *            此时 filter 到达容量上限,拒绝插入。调用方应该考虑用更大的
 *            estimated_keys 重建 filter,或者改用更大的 tag 位宽。
 *
 * 关于"每张表能扛一次踢不动":
 *   末表第一次踢不动时,tag 进 victim,insert 返回 OK,不扩容。
 *   第二次踢不动时,kick_out 一进门看到 victim 已占用,立刻 ERR → 触发扩容。
 *   所以扩容信号是"延迟一次"的,这是有意的设计:victim 本来就是给
 *   偶发的、运气不好的那一个 tag 兜底,没必要为它就扩容 4 倍内存。
 *
 * 这条路径不会丢数据:
 *   kick_out 返回 ERR 时 tag 一个字节都没写进去,紧接着由第 3 步在新表安置。
 *   全程不存在"tag 被静默丢弃"的窗口,所以 filter 始终不会产生 false negative。
 *
 * 复杂度:平均 O(1);最差 O(MAX_ITERATION × TAGS_PER_BUCKET) = 500 × 4。
 * 线程安全:无锁,不是线程安全的。多线程使用需要外部加锁。
 * -------------------------------------------------------------------------- */
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

    if (cuckoo_filter_expand(filter) != CUCKOO_OK) {//扩容失败
        return CUCKOO_ERR;
    } else {
        table = cuckoo_filter_current_table(filter);
        return cuckoo_table_insert_no_kick(table, hv); //插入到最后table
    }
}

/* -----------------------------------------------------------------------------
 * 单表查询:这个 key 有可能在这张表里吗?
 *
 * 只需要看 3 个地方(这是 cuckoo filter 查询快的根本原因):
 *   1. i1 的 4 个槽 —— 有任何一个等于 tag 就算命中
 *   2. i2 的 4 个槽 —— 同上
 *   3. victim cache —— used 且 tag 相同,且 victim.index 等于 i1 或 i2
 *      (为什么要比较 index?因为踢出链条最后停在的桶可能是 i1 也可能是 i2,
 *       两者都算合法位置;只比 tag 不比 index 会放大误判率。)
 *
 * 都没找到 → CUCKOO_ERR,含义是"这张表里一定没有"。
 *
 * 假阳性从哪来:
 *   两个不同 key 算出相同的 tag,并且其中一个恰好被放在了另一个的候选桶里。
 *   概率量级 ≈ 8 / 2^bits_per_tag(8 是候选槽数)。
 *   8-bit 时约 3%(单表满载最坏情况),实际负载下远低于此。
 *   位宽越大、负载率越低,误判越少。
 *
 * 中间那句 assert 是一道防御性检查:
 *     assert(bits_per_tag == 8 || == 12 || == 16 || == 32);
 *   确认表里存的是"真实位宽"而不是"枚举下标"(0/1/2/3)。
 *   为什么值得专门断言一次?因为对外 API 收的是枚举下标
 *   (CUCKOO_FILTER_BITS_PER_TAG_8 == 0),内部存的是真实位宽,
 *   两套数字长得很像、极容易混用。一旦混错:
 *     - read_tag / write_tag 的四个 if 全部不匹配 → 一个字节都读不出来,
 *       tag 恒为 0,查询永远返回"不存在",而且不会报任何错。
 *   这种静默失效非常难查,所以在查询入口直接断死。
 *   (历史上这里曾误写成 `if (bits_per_tag == CUCKOO_FILTER_BITS_PER_TAG_8)
 *    return CUCKOO_ERR;`,因为枚举值是 0 而位宽不可能为 0,条件恒假,
 *    是一段无效的死代码。现在换成断言,意图明确且能真正拦住错误。)
 *
 *   这里用的是标准库 <assert.h> 的 assert。历史原因是 latte_assert 所在的
 *   debug 模块当时依赖 log(进而拖进 dict/sds/utils/time),而 cuckoo 只依赖
 *   zmalloc 和 siphash,用它会把整套日志栈链进来。debug 现在已经是零依赖叶子
 *   模块(见 src/debug/lib.mk),cuckoo 也已 include 了它,可以放心换成
 *   latte_assert —— 好处是失败时会打印 backtrace 而不是只有一行 abort。
 * -------------------------------------------------------------------------- */
int cuckoo_table_contains(cuckoo_table_t* table, uint64_t hv) {
    size_t i1, i2;
    uint32_t tag;

    cuckoo_table_index_tag(table, hv, &i1, &tag);
    i2 = cuckoo_table_alt_index(table, i1, tag);

    assert(table->bits_per_tag == 8 || table->bits_per_tag == 12 || table->bits_per_tag == 16 || table->bits_per_tag == 32);

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

/* -----------------------------------------------------------------------------
 * Filter 级查询(对外 API):挨个问每张表,任意一张说"有"就返回 OK。
 *
 * 遍历顺序新 → 旧,和插入保持一致:新数据在新表,大概率第一张就命中,
 * 平均扫的表数更少。
 *
 * 语义保证(务必记牢):
 *   返回 CUCKOO_ERR → "一定没插入过",可以放心走 fast path(比如直接返回未命中)。
 *   返回 CUCKOO_OK  → "可能插入过",需要回源确认。
 * 也就是说:没有 false negative,只有 false positive。
 *
 * 这个保证靠两点撑住:
 *   1. 插入路径永不静默丢 tag —— 踢不动就进 victim,victim 满了就 ERR 让上层扩容,
 *      victim 绝不会被覆盖(见 kick_out 的第 0 步)。
 *   2. 查询路径把 victim 也算作合法位置,不会漏查。
 * 唯一能打破它的是 delete 的误删(见 cuckoo_table_delete 的说明)。
 *
 * 最坏开销:4 张表 × 2 个桶 = 8 次内存访问。
 * -------------------------------------------------------------------------- */
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

/* -----------------------------------------------------------------------------
 * 趁着 delete 腾出了空槽,试着把 victim 里那个流浪 tag 安置回主表。
 *
 * 为什么要做这件事:
 *   victim 只有一个槽,一直被占着的话,下一次踢出失败就只能覆盖它、丢数据。
 *   而且 victim 里的 tag 处于"半游离"状态,虽然可查可删,但语义特殊。
 *   删除刚好释放了一个槽 —— 这是把它请回主表的最好时机。
 *
 * 步骤:
 *   1. victim.used = 0
 *   2. ntags-- —— 抵消,因为紧接着的 kick_out 内部会再 ntags++,
 *      避免同一个 tag 被统计两次。
 *   3. 用 (victim.index, victim.tag) 走 kick_out 重新插入。
 *
 * ⚠️ 第 1 步的顺序是"不能动"的,不只是为了整洁:
 *   kick_out 进门第一件事就是 `if (victim.used) return CUCKOO_ERR;`。
 *   如果这里不先把 used 清成 0,kick_out 会立刻拒绝返回 ERR,
 *   而本函数忽略了返回值,同时 used 又已经被当成"要搬走"处理 ——
 *   结果就是那个 tag 凭空消失,产生 false negative。
 *   先清零之后,kick_out 看到的是"victim 空着",于是:
 *     - 找到空槽 → tag 回到主表,victim 真正腾空,✓
 *     - 500 轮耗尽 → 把 tag 重新写回 victim,状态原样还原,✓
 *   两条路都不会丢数据,所以这里忽略返回值是安全的。
 *
 * 注意:这不是递归/循环消化。一次 delete 只尝试一次,不会反复清理。
 * -------------------------------------------------------------------------- */
static inline void cuckoo_table_try_eliminate_victim_cache(cuckoo_table_t* table) {
    if (table->victim.used) {
        table->victim.used = 0;
        table->ntags--;
        cuckoo_table_insert_kick_out_index_tag(table, table->victim.index,
                table->victim.tag);
    }
}


/* -----------------------------------------------------------------------------
 * 单表删除:在两个候选桶里找到这个 tag,写 0 把槽清空。
 *
 * 这就是 cuckoo filter 能删、Bloom Filter 不能删的原因:
 *   Bloom 是把多个 bit 置 1,不同 key 共享 bit,清掉一个会连带影响别人。
 *   cuckoo 存的是一个个独立的指纹,清掉一个槽只影响这一条记录。
 *
 * 流程(和 contains 几乎一样,只是命中后要写 0):
 *   1. 扫 i1 的 4 槽,找到 tag → 写 CUCKOO_TAG_NULL,ntags--,
 *      顺手 try_eliminate_victim_cache(刚腾出空位,让 victim 搬进来),返回 OK。
 *   2. 扫 i2 的 4 槽,同上。
 *   3. tag 就在 victim 里(且 index 匹配 i1 或 i2)→ 直接 used = 0,ntags--,返回 OK。
 *      这条路径不用调 try_eliminate,因为被删的正是 victim 本身。
 *   4. 都没找到 → CUCKOO_ERR。
 *
 * ⚠️ 误删风险(false deletion)—— 用之前一定要想清楚:
 *   过滤器里只有指纹,没有真 key,所以它无法区分"两个不同 key 但指纹相同"。
 *   如果 key A 和 key B 恰好 tag 相同、候选桶也重叠,
 *   delete(A) 可能清掉的是 B 留下的那个槽 → 之后 contains(B) 会返回"不存在"。
 *   这就打破了"没有 false negative"的保证。
 *
 *   发生概率随 (1) tag 位数变小、(2) 负载率变高 而升高。
 *   在"缓存前置过滤"这类场景通常可接受(顶多多回源一次);
 *   但如果拿它当删除墓碑(tombstone)之类的强正确性用途,
 *   必须配合真实 key 做二次确认。
 *
 * 另外:因为插入不去重,同一个 key 插了 N 次就要删 N 次才干净。
 *
 * 返回:CUCKOO_OK = 删掉了一个匹配的指纹(有可能不是你想删的那个);
 *      CUCKOO_ERR = 这张表里没有匹配的指纹。
 * -------------------------------------------------------------------------- */
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

/* -----------------------------------------------------------------------------
 * Filter 级删除(对外 API):新 → 旧遍历,第一张删成功的表就收工返回。
 *
 * "只删一个"的语义要注意:
 *   - 如果同一个 key 在多张表里都有记录(比如插入过多次,分别落在不同表),
 *     一次 delete 只清掉最新那一份,旧表里的还在 → contains 仍然返回 OK。
 *   - 想彻底清干净,得反复 delete 到返回 CUCKOO_ERR 为止。
 *
 * 返回:CUCKOO_OK = 删掉了一份;CUCKOO_ERR = 所有表都没找到。
 * -------------------------------------------------------------------------- */
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

/* -----------------------------------------------------------------------------
 * 采集运行时统计,用来观测健康度、决定要不要调参。
 *
 * 填充的字段:
 *   ntags        所有表已用槽数之和(不含 victim)
 *   used_memory  所有表 data 的字节数之和(不含结构体本身)
 *   ntables      当前表数
 *   load_factor  整体负载率 = ntags / 所有表槽位总数
 *   load_factors 每张表各自的负载率
 *
 * 实现细节:开头 memset 清零很关键 —— load_factors 数组长度是
 *   CUCKOO_FILTER_MAX_TABLES,但只有前 ntables 项会被赋值,
 *   剩下的必须是 0 而不是栈上的垃圾值。
 *
 * ⚠️ 怎么读这些数字:
 *   刚扩容后,新表几乎是空的,而它的槽位数是所有旧表之和的 4 倍以上,
 *   所以它会把 load_factor 强行拉低。整体负载率低 ≠ 还很宽裕!
 *
 *   例:tables = [16桶满载][64桶空]
 *       ntags = 64,总槽位 = (16+64)*4 = 320 → load_factor = 0.2
 *       看着很空,但旧表其实已经一个槽都不剩了。
 *
 *   所以判断"是否接近容量上限"要看 load_factors[ntables-1](末表):
 *     < 0.8      健康,插入基本走 no_kick 快路径
 *     0.8 ~ 0.95 开始频繁 kick_out,插入延迟上升
 *     > 0.95     快满了,准备扩容;若已是第 4 张表就要考虑重建 filter
 * -------------------------------------------------------------------------- */
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

/* -----------------------------------------------------------------------------
 * 返回所有表数据区占用的总字节数。
 *
 * 只统计 data(那些大数组),不含 cuckoo_filter_t / cuckoo_table_t 结构体
 * 本身的几十字节 —— 相对于数据区可以忽略。
 *
 * 每张表的字节数 = bytes_per_bucket × nbuckets。
 *
 * 例:8-bit tag、8M 桶 → 4 字节/桶 × 8M = 32 MB,可容纳 32M 个 tag。
 *
 * 和 cuckoo_filter_get_stat 里的 used_memory 是同一个值;
 * 这个函数是只要内存数字时的轻量版本,不用构造 stat 结构体。
 * -------------------------------------------------------------------------- */
size_t cuckoo_filter_used_memory(cuckoo_filter_t* filter) {
    size_t used_memory = 0;
    for (int i = 0; i < filter->ntables; i++) {
        cuckoo_table_t* table = filter->tables+i;
        used_memory += table->bytes_per_bucket * table->nbuckets;
    }
    return used_memory;
}
