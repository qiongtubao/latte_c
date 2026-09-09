
#include "../test/testhelp.h"
#include "../test/testassert.h"
#include "cuckoo.h"


int test_cuckoo() {
    assert(is_pow_of_2(1) == 1);
    assert(is_pow_of_2(2) == 1);
    assert(is_pow_of_2(3) == 0);
    assert(is_pow_of_2(4) == 1);
    assert(is_pow_of_2(5) == 0);
    assert(is_pow_of_2(255) == 0);
    assert(is_pow_of_2(256) == 1);

    assert(upper_pow_of_2(1) == 1);
    assert(upper_pow_of_2(2) == 2);
    assert(upper_pow_of_2(3) == 4);
    assert(upper_pow_of_2(4) == 4);
    assert(upper_pow_of_2(5) == 8);
    assert(upper_pow_of_2(6) == 8);
    return 1;
}

/* -----------------------------------------------------------------------------
 * 反复插入"同一个 key"会在第 10 次触发扩容。
 *
 * 为什么是 10:
 *   同一个 key → 同一个 hv → 在每张表里 i1 / i2 / tag 全部固定,
 *   所以它在一张表里只有 8 个合法槽位(i1 的 4 个 + i2 的 4 个)。
 *     第 1~8 次:no_kick 逐个填满这 8 个槽。
 *     第 9   次:no_kick 失败 → kick_out。踢出链在同一个 tag 上空转
 *               (otag 读出来就是 tag 本身,写回等于没变,i 在 i1/i2 间来回弹),
 *               500 轮耗尽后寄存进 victim,仍返回 OK,不扩容。
 *     第 10  次:no_kick 失败 → kick_out 进门看到 victim.used 直接 ERR
 *               → insert 认为末表已满 → expand,ntables 从 1 变 2。
 *
 * 这条路径说明:filter 不去重(见 cuckoo.c 头部"坑 a")的后果不只是浪费槽位,
 * 单个热点 key 就能伪造出"表满了"的信号,把内存放大到 85 倍
 * (4 张表,每张是前一张的 4 倍),而扩容那一刻真实负载率不到 1%。
 * -------------------------------------------------------------------------- */
int test_cuckoo_same_key_expand() {
    const char *key = "same-key";
    size_t klen = strlen(key);

    cuckoo_filter_t *filter = cuckoo_filter_new(cuckoo_gen_hash_function,
            CUCKOO_FILTER_BITS_PER_TAG_8, 1000);
    assert(filter != NULL);
    assert(filter->ntables == 1);
    size_t first_nbuckets = filter->tables[0].nbuckets;

    /* 第 1~8 次:填满 i1 / i2 的 8 个槽,始终留在首表,victim 未启用。 */
    for (int n = 1; n <= CUCKOO_FILTER_TAGS_PER_BUCKET * 2; n++) {
        assert(cuckoo_filter_insert(filter, key, klen) == CUCKOO_OK);
        assert(filter->ntables == 1);
        assert(filter->tables[0].victim.used == 0);
        assert(filter->tables[0].ntags == (size_t)n);
    }

    /* 第 9 次:踢不动,寄存进 victim。仍然是 OK,仍然不扩容。 */
    assert(cuckoo_filter_insert(filter, key, klen) == CUCKOO_OK);
    assert(filter->ntables == 1);
    assert(filter->tables[0].victim.used == 1);
    assert(filter->tables[0].ntags == 9);

    /* 第 10 次:victim 已被占用 → kick_out 直接 ERR → 扩容,新表是旧表的 4 倍。 */
    assert(cuckoo_filter_insert(filter, key, klen) == CUCKOO_OK);
    assert(filter->ntables == 2);
    assert(filter->tables[1].nbuckets ==
            first_nbuckets * CUCKOO_FILTER_BUCKETS_EXPANSION);
    /* 首表状态原封不动 —— 扩容不搬迁旧数据。 */
    assert(filter->tables[0].nbuckets == first_nbuckets);
    assert(filter->tables[0].ntags == 9);
    assert(filter->tables[0].victim.used == 1);
    /* 这次插入落在新表的第一个空槽里。 */
    assert(filter->tables[1].ntags == 1);
    assert(filter->tables[1].victim.used == 0);

    cuckoo_filter_free(filter);
    return 1;
}

/* -----------------------------------------------------------------------------
 * 同一个 key 一直插下去:每张表撑 9 次,4 张表用完后彻底拒绝插入。
 *
 * 时间线(接着上面的第 10 次):
 *   #10 expand→表1  #11~17 填表1 槽  #18 表1 victim  #19 expand→表2
 *   #28 expand→表3(ntables 达 CUCKOO_FILTER_MAX_TABLES)
 *   #36 表3 victim  #37 起 expand 失败 → CUCKOO_ERR
 *
 * 所以重复插同一个 key 的上限是 9 × MAX_TABLES = 36 次。
 * 注意最后仍然 contains == OK:插入被拒绝时 tag 一个字节都没写进去,
 * 之前那 36 个副本还在,filter 依旧没有 false negative。
 * -------------------------------------------------------------------------- */
int test_cuckoo_same_key_expand_until_full() {
    const char *key = "same-key";
    size_t klen = strlen(key);
    /* 每张表能吃下的次数 = 8 个槽 + 1 个 victim。 */
    const int per_table = CUCKOO_FILTER_TAGS_PER_BUCKET * 2 + 1;

    cuckoo_filter_t *filter = cuckoo_filter_new(cuckoo_gen_hash_function,
            CUCKOO_FILTER_BITS_PER_TAG_8, 1000);
    assert(filter != NULL);

    int expands = 0;
    int prev_ntables = filter->ntables;
    for (int n = 1; n <= per_table * CUCKOO_FILTER_MAX_TABLES; n++) {
        assert(cuckoo_filter_insert(filter, key, klen) == CUCKOO_OK);
        if (filter->ntables != prev_ntables) {
            expands++;
            /* 每次扩容只加一张表,且新表是前一张的 4 倍。 */
            assert(filter->ntables == prev_ntables + 1);
            assert(filter->tables[filter->ntables-1].nbuckets ==
                    filter->tables[filter->ntables-2].nbuckets *
                    CUCKOO_FILTER_BUCKETS_EXPANSION);
            /* 扩容一定发生在"第 10 次、第 19 次…"这些位置上。 */
            assert(n % per_table == 1);
            prev_ntables = filter->ntables;
        }
    }

    /* 首表之外的每张表都是靠一次扩容来的。 */
    assert(expands == CUCKOO_FILTER_MAX_TABLES - 1);
    assert(filter->ntables == CUCKOO_FILTER_MAX_TABLES);
    assert(filter->tables[CUCKOO_FILTER_MAX_TABLES-1].victim.used == 1);

    /* 表数已达上限,expand 失败 → 插入被拒绝,而且是稳定拒绝。 */
    for (int n = 0; n < 3; n++) {
        assert(cuckoo_filter_insert(filter, key, klen) == CUCKOO_ERR);
        assert(filter->ntables == CUCKOO_FILTER_MAX_TABLES);
    }

    /* 被拒绝的插入不写任何数据,已插入的副本仍可查到。 */
    assert(cuckoo_filter_contains(filter, key, klen) == CUCKOO_OK);

    cuckoo_filter_free(filter);
    return 1;
}

/* =============================================================================
 * 误删 (false deletion) 场景
 * =============================================================================
 *
 * ## 先说清楚"碰撞"要碰撞到什么程度
 *
 * 让两个 key 的完整 64 位 hv 相同是 2^64 量级的碰撞,搜不出来,也没必要。
 * 误删只依赖 hv 里"被实际用到"的那些位:
 *
 *     i1  = (hv >> 32) & (nbuckets - 1)       ← 只用到高 32 位的低 log2(nbuckets) 位
 *     tag = (hv & 0xFFFFFFFF) & 0xFF          ← 8-bit tag 只用到最低 8 位
 *     i2  = alt(i1, tag)                      ← 由 i1 和 tag 推出,不需要额外碰撞
 *
 * 所以只要 (i1, tag) 两项吻合,两个 key 在这张表里就完全无法区分。
 * 用最小表(CUCKOO_FILTER_TABLE_MIN_BUCKETS = 16 桶)+ 8-bit tag,
 * 需要吻合的只有 4 + 8 = 12 位 → 期望约 2^12 = 4096 次尝试。
 *
 * ## 本测试用的碰撞对(离线搜出来的)
 *
 * 搜索方式:固定 A = "victim-key",遍历 "k0", "k1", "k2", ... 直到
 *          i1 和 tag 都与 A 相同。第 585 个就命中了。
 *
 *     A = "victim-key"  hv = 0xec7e4c655f82c86f   i1 = 5  i2 = 14  tag = 111
 *     B = "k585"        hv = 0xe21fe17520b3af6f   i1 = 5  i2 = 14  tag = 111
 *                            ^^^^^^^^         ^^
 *                            hv 完全不同    但低字节 0x6f 相同 → tag 相同
 *                                           且高 32 位的低 4 位都是 0x5 → i1 相同
 *
 * ⚠️ 这对 key 依赖 cuckoo.c 里硬编码的 cuckoo_hash_function_seed。
 *   如果哪天改成进程启动随机填充 seed(cuckoo.c 注释里提到的抗 hash flooding
 *   方案),下面的 precondition 断言会立刻失败,需要用同样的方式重新搜一对。
 * -------------------------------------------------------------------------- */
#define FALSE_DEL_KEY_A "victim-key"   /* 从未被插入,只用来"删" */
#define FALSE_DEL_KEY_B "k585"         /* 真正插入的 key,却被 A 的删除干掉 */

/* 复刻 cuckoo.c 的内部计算,仅用于在测试里显式校验碰撞前提。
 * 这里刻意硬编码 8-bit tag,因为下面的碰撞对就是按 8-bit 搜出来的。 */
static void cuckoo_test_index_tag(uint64_t hv, size_t nbuckets,
        size_t *i1, uint32_t *tag) {
    *i1 = (hv >> 32) & (nbuckets - 1);
    *tag = (hv & 0xFFFFFFFF) & 0xFF;
    *tag += (*tag == 0);
}

static size_t cuckoo_test_alt_index(size_t i1, uint32_t tag, size_t nbuckets) {
    return (i1 ^ ((size_t) tag * 0x5bd1e995)) & (nbuckets - 1);
}

/* 校验两个 key 在 nbuckets 桶的表里确实无法区分:i1、i2、tag 三项全等。
 * 返回 1 表示碰撞前提成立。 */
static int cuckoo_test_keys_collide(const char *ka, const char *kb, size_t nbuckets) {
    uint64_t hva = cuckoo_gen_hash_function(ka, strlen(ka));
    uint64_t hvb = cuckoo_gen_hash_function(kb, strlen(kb));
    size_t ia1, ia2, ib1, ib2;
    uint32_t ta, tb;

    cuckoo_test_index_tag(hva, nbuckets, &ia1, &ta);
    cuckoo_test_index_tag(hvb, nbuckets, &ib1, &tb);
    ia2 = cuckoo_test_alt_index(ia1, ta, nbuckets);
    ib2 = cuckoo_test_alt_index(ib1, tb, nbuckets);

    /* hv 本身必须不同 —— 否则就不是"两个不同 key 的指纹碰撞",
     * 而是同一个哈希值,那就失去了这个测试的意义。 */
    if (hva == hvb) return 0;
    return ta == tb && ia1 == ib1 && ia2 == ib2;
}

/* 比 keys_collide 宽松一档:只要求 tag 相同、两个候选桶构成的**集合**相同,
 * 不要求 i1 对上 i1。跨表场景需要这个版本 —— 同一对桶在两个 key 身上
 * 主备角色可能是互换的(X 是 {47,0},Y 是 {0,47}),但可放置位置完全一致。
 * 返回 1 表示这两个 key 在该表里不可区分。 */
static int cuckoo_test_keys_share_buckets(const char *ka, const char *kb,
        size_t nbuckets) {
    uint64_t hva = cuckoo_gen_hash_function(ka, strlen(ka));
    uint64_t hvb = cuckoo_gen_hash_function(kb, strlen(kb));
    size_t ia1, ia2, ib1, ib2;
    uint32_t ta, tb;

    cuckoo_test_index_tag(hva, nbuckets, &ia1, &ta);
    cuckoo_test_index_tag(hvb, nbuckets, &ib1, &tb);
    ia2 = cuckoo_test_alt_index(ia1, ta, nbuckets);
    ib2 = cuckoo_test_alt_index(ib1, tb, nbuckets);

    if (hva == hvb || ta != tb) return 0;
    return (ia1 == ib1 && ia2 == ib2) || (ia1 == ib2 && ia2 == ib1);
}

/* 判断两个 key 在该表里"完全碰不到":tag 不同,或者候选桶两两不相交。
 * 用来挑一个绝不会干扰主角的填充 key。返回 1 表示互不干扰。 */
static int cuckoo_test_keys_independent(const char *ka, const char *kb,
        size_t nbuckets) {
    uint64_t hva = cuckoo_gen_hash_function(ka, strlen(ka));
    uint64_t hvb = cuckoo_gen_hash_function(kb, strlen(kb));
    size_t ia1, ia2, ib1, ib2;
    uint32_t ta, tb;

    cuckoo_test_index_tag(hva, nbuckets, &ia1, &ta);
    cuckoo_test_index_tag(hvb, nbuckets, &ib1, &tb);
    ia2 = cuckoo_test_alt_index(ia1, ta, nbuckets);
    ib2 = cuckoo_test_alt_index(ib1, tb, nbuckets);

    if (ta != tb) return 1;   /* tag 不同,读到也不会误判为匹配 */
    return ia1 != ib1 && ia1 != ib2 && ia2 != ib1 && ia2 != ib2;
}

/* -----------------------------------------------------------------------------
 * 误删主场景:删除一个从未插入过的 key,把另一个真实存在的 key 抹掉。
 *
 * 这是 cuckoo filter 最危险的一个性质,它同时打破了两条直觉:
 *   1. delete 一个没插过的 key,居然返回成功。
 *   2. filter 号称"绝不产生 false negative",但 delete 之后真的产生了。
 *      (contains 的保证只在"没有误删"的前提下成立,见 cuckoo.c 里的坑 b)
 *
 * 桶 5 的状态变化(只插了 B):
 *
 *   insert(B)        bucket5 [111][ 0 ][ 0 ][ 0 ]     111 是 B 的指纹
 *   contains(A)  →   扫桶 5 看到 111 == A 的 tag → 报"存在"(假阳性)
 *   delete(A)    →   扫桶 5 找到 111 → 写 0,返回成功
 *                    bucket5 [ 0 ][ 0 ][ 0 ][ 0 ]     B 的指纹被抹掉了
 *   contains(B)  →   桶 5、桶 14 全空 → 报"不存在"(假阴性!)
 * -------------------------------------------------------------------------- */
int test_cuckoo_false_deletion() {
    const char *ka = FALSE_DEL_KEY_A, *kb = FALSE_DEL_KEY_B;
    size_t la = strlen(ka), lb = strlen(kb);

    /* estimated_keys 取小值,让首表落在 16 桶的下限上 —— 碰撞对就是按 16 桶搜的。 */
    cuckoo_filter_t *filter = cuckoo_filter_new(cuckoo_gen_hash_function,
            CUCKOO_FILTER_BITS_PER_TAG_8, 10);
    assert(filter != NULL);
    assert(filter->ntables == 1);
    assert(filter->tables[0].nbuckets == CUCKOO_FILTER_TABLE_MIN_BUCKETS);

    /* precondition:两个 key 在这张表里确实不可区分。不成立则后面的断言毫无意义。 */
    assert(cuckoo_test_keys_collide(ka, kb, filter->tables[0].nbuckets) == 1);

    /* 空 filter:两个 key 都不存在,删也删不掉。 */
    assert(cuckoo_filter_contains(filter, ka, la) == CUCKOO_ERR);
    assert(cuckoo_filter_contains(filter, kb, lb) == CUCKOO_ERR);
    assert(cuckoo_filter_delete(filter, ka, la) == CUCKOO_ERR);

    /* 只插入 B。 */
    assert(cuckoo_filter_insert(filter, kb, lb) == CUCKOO_OK);
    assert(filter->tables[0].ntags == 1);
    assert(cuckoo_filter_contains(filter, kb, lb) == CUCKOO_OK);

    /* 假阳性:A 从没插入过,却被报告"存在"。 */
    assert(cuckoo_filter_contains(filter, ka, la) == CUCKOO_OK);

    /* 误删:删 A 居然成功,因为它在桶里看到的是 B 留下的指纹。 */
    assert(cuckoo_filter_delete(filter, ka, la) == CUCKOO_OK);
    assert(filter->tables[0].ntags == 0);

    /* 后果:B 明明插入过且没被删过,现在查不到了 —— 这就是 false negative。 */
    assert(cuckoo_filter_contains(filter, kb, lb) == CUCKOO_ERR);

    /* 指纹已经被清掉,再删就没得删了。 */
    assert(cuckoo_filter_delete(filter, ka, la) == CUCKOO_ERR);
    assert(cuckoo_filter_delete(filter, kb, lb) == CUCKOO_ERR);

    cuckoo_filter_free(filter);
    return 1;
}

/* -----------------------------------------------------------------------------
 * 误删的另一面:delete 不幂等,而且"删几次"取决于槽里有几个同 tag 的副本。
 *
 * A 和 B 都插进去之后,桶 5 里有两个一模一样的 111,filter 根本分不清哪个是谁。
 * 于是"删 A"两次就能把 B 也删掉:
 *
 *   insert(A); insert(B)   bucket5 [111][111][ 0 ][ 0 ]
 *   delete(A) #1  → 清掉 slot0,contains(B) 仍然 OK(slot1 还在)
 *   delete(A) #2  → 清掉 slot1,contains(B) 变成 ERR
 *   delete(A) #3  → 没得删了,ERR
 *
 * 换个角度看:这两次 delete 里至少有一次删错了对象,但 API 无法告诉你是哪一次。
 * 所以拿 cuckoo filter 当"删除墓碑"之类的强正确性用途时,
 * 必须用真实 key 做二次确认。
 * -------------------------------------------------------------------------- */
int test_cuckoo_false_deletion_not_idempotent() {
    const char *ka = FALSE_DEL_KEY_A, *kb = FALSE_DEL_KEY_B;
    size_t la = strlen(ka), lb = strlen(kb);

    cuckoo_filter_t *filter = cuckoo_filter_new(cuckoo_gen_hash_function,
            CUCKOO_FILTER_BITS_PER_TAG_8, 10);
    assert(filter != NULL);
    assert(cuckoo_test_keys_collide(ka, kb, filter->tables[0].nbuckets) == 1);

    /* 两个 key 都插入 → 同一个桶里两个相同的指纹,占 2 个槽(filter 不去重)。 */
    assert(cuckoo_filter_insert(filter, ka, la) == CUCKOO_OK);
    assert(cuckoo_filter_insert(filter, kb, lb) == CUCKOO_OK);
    assert(filter->tables[0].ntags == 2);

    /* 第 1 次删 A:B 还能查到,因为还剩一个同 tag 的副本。 */
    assert(cuckoo_filter_delete(filter, ka, la) == CUCKOO_OK);
    assert(filter->tables[0].ntags == 1);
    assert(cuckoo_filter_contains(filter, kb, lb) == CUCKOO_OK);

    /* 第 2 次删 A(A 只插了一次!)也成功,这一次抹掉的是 B 的记录。 */
    assert(cuckoo_filter_delete(filter, ka, la) == CUCKOO_OK);
    assert(filter->tables[0].ntags == 0);
    assert(cuckoo_filter_contains(filter, kb, lb) == CUCKOO_ERR);

    /* 第 3 次:槽已清空,删不动了。 */
    assert(cuckoo_filter_delete(filter, ka, la) == CUCKOO_ERR);

    /* 对照组:一个不碰撞的 key 完全不受影响,误删只波及"同 tag 同桶"的 key。 */
    const char *other = "unrelated-key";
    size_t lo = strlen(other);
    assert(cuckoo_test_keys_collide(ka, other, filter->tables[0].nbuckets) == 0);
    assert(cuckoo_filter_insert(filter, other, lo) == CUCKOO_OK);
    assert(cuckoo_filter_delete(filter, ka, la) == CUCKOO_ERR);
    assert(cuckoo_filter_contains(filter, other, lo) == CUCKOO_OK);

    cuckoo_filter_free(filter);
    return 1;
}

/* =============================================================================
 * 跨表误删:delete 新→旧遍历 + 第一张删成功就 return 带来的泄漏
 * =============================================================================
 *
 * ## 机制
 *
 * cuckoo_filter_delete 是这样写的:
 *     for (i = ntables-1; i >= 0; i--)
 *         if (cuckoo_table_delete(tables+i, hv) == CUCKOO_OK) return CUCKOO_OK;
 *
 * 于是:X 真实存在于 table0(扩容前插入的老 key),delete(X) 先扫 table1。
 * 只要 table1 里 X 的 8 个候选槽有任意一个恰好是同一个 tag(其实属于另一个
 * key Y),就把 Y 的 tag 删掉、立刻 return OK,**根本不会往下扫 table0**。
 *
 * 后果一(泄漏):X 的 tag 原封不动留在 table0,继续贡献假阳性。
 *   调用方拿到 OK 就认为删干净了,不会再删第二次 —— 这就是泄漏。
 *   严格说不是"永久":下一次对 X 的 delete 会清掉它。但语义上调用方
 *   没有理由再删一次,所以实践中等于泄漏。
 *
 * 后果二(Y 被删):Y 明明没被删过,它在 table1 的指纹却被抹掉了。
 *
 * ## 一个反直觉的修正:Y 不会**立刻**变成 false negative
 *
 * tag 的计算 (hv & 0xFFFFFFFF) & mask 完全不依赖 nbuckets,
 * 所以 X 在每张表里 tag 都一样;而 i1 只是低位截断:
 *
 *     i1_t0 = (hv >> 32) & 15        i1_t1 = (hv >> 32) & 63
 *     ⇒ i1_t0 == i1_t1 & 15
 *
 * 再加上 alt() 用的是同一个 tag:
 *
 *     i2_t1 & 15 = (i1_t1 ^ h) & 15 = (i1_t0 ^ h) & 15 = i2_t0
 *
 * 结论:**任何能在 table1 被 X 误删的 Y,必然在 table0 里和 X 共享同一对
 * 候选桶**。而 X 泄漏的那个 tag 正躺在这对桶里,tag 又相同 ——
 * 所以 contains(Y) 会撞上 X 的残留 tag,依旧返回 OK。
 * Y 被 X 的泄漏"掩护"住了,两个 bug 在这一步互相抵消。
 *
 * 本例的实测数据(table0 = 16 桶,table1 = 64 桶,tag 8 位):
 *     X = "old-key"  tag=243  t0:{15,0}  t1:{47,0}
 *     Y = "y4404"    tag=243  t0:{0,15}  t1:{0,47}
 *                             ^^^^^^^^ 与 X 同一对桶,印证上面的推导
 *
 * ## 真正的 false negative 出现在第二次 delete(X)
 *
 * 第 2 次 delete(X):table1 已经没有匹配的 tag 了,于是落到 table0,
 * 把 X 自己的 tag 删掉。这一刻掩护消失 —— Y 的所有可能位置都空了:
 *
 *     contains(Y) → CUCKOO_ERR,而 Y 从未被删除过。
 *
 * 也就是说,X 只插入了一次,却被成功删除了两次(两次都返回 OK),
 * 代价是 Y 无声消失。对上层(比如 rocksdb 前置过滤)而言,
 * Y 的数据真实存在于底层存储,filter 却说"一定不存在" → 查询直接被跳过。
 *
 * ## 构造手法
 *
 * 需要在"插入 X"和"插入 Y"之间制造一次扩容。这里借用前面验证过的性质:
 * 同一个 key 连插 10 次必然触发扩容(见 test_cuckoo_same_key_expand)。
 * 填充 key F 选得与 X / Y 互不干扰(tag 不同,候选桶也不相交),
 * 所以它只负责把 table0 顶到扩容,不影响任何断言。
 * -------------------------------------------------------------------------- */
#define CROSS_DEL_KEY_X "old-key"   /* 扩容前插入,只存在于 table0 */
#define CROSS_DEL_KEY_Y "y4404"     /* 扩容后插入,只存在于 table1,与 X 同 tag 同桶 */
#define CROSS_DEL_KEY_F "f1"        /* 填充 key,只用来触发扩容 */

int test_cuckoo_cross_table_false_deletion() {
    const char *kx = CROSS_DEL_KEY_X, *ky = CROSS_DEL_KEY_Y, *kf = CROSS_DEL_KEY_F;
    size_t lx = strlen(kx), ly = strlen(ky), lf = strlen(kf);

    cuckoo_filter_t *filter = cuckoo_filter_new(cuckoo_gen_hash_function,
            CUCKOO_FILTER_BITS_PER_TAG_8, 10);
    assert(filter != NULL);
    assert(filter->ntables == 1);

    size_t nb0 = filter->tables[0].nbuckets;
    size_t nb1 = nb0 * CUCKOO_FILTER_BUCKETS_EXPANSION;
    assert(nb0 == CUCKOO_FILTER_TABLE_MIN_BUCKETS);

    /* precondition ①:X 和 Y 在 table1 里不可区分 —— 这是误删能发生的前提。 */
    assert(cuckoo_test_keys_share_buckets(kx, ky, nb1) == 1);
    /* precondition ②:在 table0 里它们也共享同一对桶。
     * 这不是巧合而是数学必然(见上面的推导),正因如此才有"掩护"效应。 */
    assert(cuckoo_test_keys_share_buckets(kx, ky, nb0) == 1);
    /* precondition ③:填充 key 与两位主角在两张表里都互不干扰。 */
    assert(cuckoo_test_keys_independent(kf, kx, nb0) == 1);
    assert(cuckoo_test_keys_independent(kf, ky, nb0) == 1);
    assert(cuckoo_test_keys_independent(kf, kx, nb1) == 1);
    assert(cuckoo_test_keys_independent(kf, ky, nb1) == 1);

    /* --- 阶段 1:扩容前插入 X,它只存在于 table0 --- */
    assert(cuckoo_filter_insert(filter, kx, lx) == CUCKOO_OK);
    assert(filter->ntables == 1);
    assert(filter->tables[0].ntags == 1);

    /* --- 阶段 2:用填充 key 把 table0 顶到扩容 --- */
    for (int n = 0; n < CUCKOO_FILTER_TAGS_PER_BUCKET * 2 + 1; n++) {
        assert(cuckoo_filter_insert(filter, kf, lf) == CUCKOO_OK);
    }
    assert(filter->ntables == 1);            /* 第 9 次进 victim,还没扩容 */
    assert(filter->tables[0].victim.used == 1);
    assert(filter->tables[0].ntags == 10);   /* X 1 个 + F 8 槽 + F 1 victim */

    assert(cuckoo_filter_insert(filter, kf, lf) == CUCKOO_OK);
    assert(filter->ntables == 2);            /* 第 10 次触发扩容 */
    assert(filter->tables[1].nbuckets == nb1);
    assert(filter->tables[0].ntags == 10);   /* 旧表原封不动,X 还在里面 */
    assert(filter->tables[1].ntags == 1);    /* 这次的 F 落在新表 */

    /* --- 阶段 3:扩容后插入 Y,它只存在于 table1 --- */
    assert(cuckoo_filter_insert(filter, ky, ly) == CUCKOO_OK);
    assert(filter->tables[0].ntags == 10);   /* Y 没进旧表 */
    assert(filter->tables[1].ntags == 2);    /* F + Y */

    assert(cuckoo_filter_contains(filter, kx, lx) == CUCKOO_OK);
    assert(cuckoo_filter_contains(filter, ky, ly) == CUCKOO_OK);

    /* --- 阶段 4:delete(X) 第 1 次 —— 删错了对象,而且漏掉了自己 --- */
    assert(cuckoo_filter_delete(filter, kx, lx) == CUCKOO_OK);
    /* 被删的是 table1 里 Y 的 tag。 */
    assert(filter->tables[1].ntags == 1);
    /* 泄漏:table0 一个 tag 都没少,X 的指纹还在。 */
    assert(filter->tables[0].ntags == 10);

    /* X 还查得到 —— 因为它真的还在 table0。调用方却已经认为删掉了。 */
    assert(cuckoo_filter_contains(filter, kx, lx) == CUCKOO_OK);
    /* Y 也还查得到,但已经不是"因为 Y 自己在" —— 它的 tag 已被抹掉,
     * 这个 OK 是 X 泄漏在 table0 的残留 tag 顶上来的(掩护效应)。 */
    assert(cuckoo_filter_contains(filter, ky, ly) == CUCKOO_OK);

    /* --- 阶段 5:delete(X) 第 2 次 —— 掩护消失,Y 变成 false negative --- */
    /* X 只插入过一次,却第二次删除依然成功。 */
    assert(cuckoo_filter_delete(filter, kx, lx) == CUCKOO_OK);
    assert(filter->tables[0].ntags == 9);    /* 这次真的把 X 从 table0 删掉了 */
    assert(filter->tables[1].ntags == 1);    /* 新表不再有匹配项 */

    assert(cuckoo_filter_contains(filter, kx, lx) == CUCKOO_ERR);
    /* ⚠️ 核心断言:Y 从未被删除过,现在却"一定不存在"。
     * 上层若据此跳过 rocksdb 查询,就会漏掉真实存在的数据。 */
    assert(cuckoo_filter_contains(filter, ky, ly) == CUCKOO_ERR);

    /* 对照:不相干的填充 key 全程不受影响。 */
    assert(cuckoo_filter_contains(filter, kf, lf) == CUCKOO_OK);

    cuckoo_filter_free(filter);
    return 1;
}

/* =============================================================================
 * 缩容 (shrink)
 * =============================================================================
 *
 * 缩容之所以可行,靠的是"新桶号是旧桶号的低位截断"这一点:
 *
 *     i1' = (hv >> 32) & (N'-1) = i1 & (N'-1)      N' = N / 2^k
 *
 * 右边只用到 i1,不需要 hv,所以光凭表内信息就能算。扩容需要 i1 里已经
 * 丢掉的高位,恢复不出来 —— 这就是经典 cuckoo filter "不能 resize"的真正含义。
 *
 * 迁移时不知道 tag 当前在主桶还是备桶也没关系,由 alt() 的对称性,
 * 两种情况推出的候选桶集合 { i & M', (i ^ h) & M' } 完全相同。
 * 完整推导见 cuckoo.c 里 cuckoo_table_migrate_tags 的注释。
 *
 * 下面五个测试分别覆盖:绝不动基表、合并末表、装不下时拒绝、
 * 迁移中途失败时回滚、victim 迁移。
 * -------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
 * ⚠️ 只摘表,绝不动基表 —— 这是缩容语义的边界,必须钉死。
 *
 * 场景:32768 桶的单表里插 20000 个 key,删掉 19000,只剩 1000。
 * 负载率掉到 1000/131072 ≈ 0.8%,内存几乎全在空转 —— 看起来"该缩了",
 * 但因为只剩基表,shrink 必须原地拒绝。
 *
 * 为什么不能缩基表:
 *   1. tables[0] 的尺寸是调用方通过 estimated_keys 声明的容量意图。
 *   2. 它是阶梯基准。expand 用"末表 × 4"算新表尺寸,把基表从 32768
 *      缩成 512 之后,下一次 expand 造出的是 2048 桶而不是 131072,
 *      再也长不回原来的阶梯 —— 可逆性丢失,总容量上限被永久压低。
 *   3. 缩基表不减少 ntables,对查询开销 O(ntables) 和 FPP 的表数因子
 *      毫无贡献,恰好丢掉了"合并末表"这个设计的全部收益。
 *
 * 想回收这部分内存只能重建 filter,那是调用方的决策。
 * 本测试同时确认:被拒绝之后 filter 依旧完好可用。
 * -------------------------------------------------------------------------- */
int test_cuckoo_shrink_never_touches_base_table() {
    const long total = 20000, keep = 1000;
    char buf[32];

    cuckoo_filter_t *filter = cuckoo_filter_new(cuckoo_gen_hash_function,
            CUCKOO_FILTER_BITS_PER_TAG_8, 100000);
    assert(filter != NULL);
    assert(filter->tables[0].nbuckets == 32768);
    size_t mem_before = cuckoo_filter_used_memory(filter);

    for (long i = 0; i < total; i++) {
        int len = snprintf(buf, sizeof(buf), "k:%ld", i);
        assert(cuckoo_filter_insert(filter, buf, len) == CUCKOO_OK);
    }
    /* 20000 个 tag 装在 131072 个槽里,不该触发扩容。 */
    assert(filter->ntables == 1);
    assert(filter->tables[0].ntags == (size_t)total);

    for (long i = keep; i < total; i++) {
        int len = snprintf(buf, sizeof(buf), "k:%ld", i);
        assert(cuckoo_filter_delete(filter, buf, len) == CUCKOO_OK);
    }
    assert(filter->tables[0].ntags == (size_t)keep);
    /* delete 只置 0,不还内存。 */
    assert(cuckoo_filter_used_memory(filter) == mem_before);

    /* ⚠️ 核心断言:负载率只有 0.8%,但只剩基表 → 一律拒绝,反复调也拒绝。 */
    for (int i = 0; i < 5; i++) {
        assert(cuckoo_filter_shrink(filter) == CUCKOO_ERR);
        assert(filter->ntables == 1);
        assert(filter->tables[0].nbuckets == 32768);            /* 尺寸没动 */
        assert(filter->tables[0].ntags == (size_t)keep);
        assert(cuckoo_filter_used_memory(filter) == mem_before); /* 内存没动 */
    }

    /* 剩下的 key 一个都不能丢。 */
    for (long i = 0; i < keep; i++) {
        int len = snprintf(buf, sizeof(buf), "k:%ld", i);
        assert(cuckoo_filter_contains(filter, buf, len) == CUCKOO_OK);
    }

    /* 缩容后的表必须还是一张正常可用的表:能插、能查、能删。 */
    assert(cuckoo_filter_insert(filter, "after-shrink", 12) == CUCKOO_OK);
    assert(cuckoo_filter_contains(filter, "after-shrink", 12) == CUCKOO_OK);
    assert(cuckoo_filter_delete(filter, "after-shrink", 12) == CUCKOO_OK);

    cuckoo_filter_free(filter);
    return 1;
}

/* -----------------------------------------------------------------------------
 * 迁移**中途**失败时必须整体回滚 —— 事务性里最难的那条路径。
 *
 * O(1) 预检只看"槽位数够不够",它通过了也不代表一定搬得进去:
 * 临时表只有一个 victim 槽,踢出链耗尽时第二个安置不下的 tag 就会失败。
 * 这时临时表已经被写了一半,如果实现是"直接往前表里插",此刻就已经
 * 丢了一半 tag —— 静默产生 false negative。正确做法是写临时表,
 * 失败就把临时表整个丢掉,两张原表全程只读。
 *
 * 构造方式(利用同 key 重复插入的确定性行为):
 *   16 桶基表,同一个 key A 连插 10 次
 *     第 1~8 次  填满 A 在基表的 8 个候选槽
 *     第 9 次    踢不动 → 进基表 victim         → 基表 ntags=9, victim=1
 *     第 10 次   基表 victim 已占 → expand      → 末表(64 桶)放 1 个 A
 *
 *   此时 shrink 走分支 B:
 *     O(1) 预检  merged = 9 + 1 = 10 ≤ 前表槽位 64  → 通过,真的开始迁移
 *     搬前表     8 个 tag 填满临时表 A 的一对候选桶,第 9 个(victim)
 *                踢 500 轮无果 → 占用临时表的 victim
 *     搬末表     末表那个 A 截断后落到同一对桶,槽满且 victim 已占 → ERR
 *     → 回滚
 *
 * 断言:返回 CUCKOO_ERR,且 ntables / 各表桶数 / ntags / victim / 内存
 * 全部逐项不变,A 仍然查得到。
 * -------------------------------------------------------------------------- */
int test_cuckoo_shrink_rolls_back_on_migration_failure() {
    const char *key = "rollback-key";
    size_t klen = strlen(key);

    cuckoo_filter_t *filter = cuckoo_filter_new(cuckoo_gen_hash_function,
            CUCKOO_FILTER_BITS_PER_TAG_8, 10);
    assert(filter != NULL);
    assert(filter->tables[0].nbuckets == CUCKOO_FILTER_TABLE_MIN_BUCKETS);

    for (int i = 0; i < CUCKOO_FILTER_TAGS_PER_BUCKET * 2 + 2; i++)
        assert(cuckoo_filter_insert(filter, key, klen) == CUCKOO_OK);

    assert(filter->ntables == 2);
    assert(filter->tables[0].ntags == 9);          /* 8 个槽 + 1 个 victim */
    assert(filter->tables[0].victim.used == 1);
    assert(filter->tables[1].ntags == 1);

    /* O(1) 预检会通过 —— 所以下面走的是"真的迁移然后失败"这条路径,
     * 而不是被预检提前挡掉。 */
    size_t dst_slots = filter->tables[0].nbuckets * CUCKOO_FILTER_TAGS_PER_BUCKET;
    assert(filter->tables[0].ntags + filter->tables[1].ntags <= dst_slots);

    size_t nb0 = filter->tables[0].nbuckets, nb1 = filter->tables[1].nbuckets;
    size_t n0 = filter->tables[0].ntags, n1 = filter->tables[1].ntags;
    int v0 = filter->tables[0].victim.used, v1 = filter->tables[1].victim.used;
    size_t mem = cuckoo_filter_used_memory(filter);

    /* ⚠️ 核心断言:失败,且逐项完全没变。 */
    assert(cuckoo_filter_shrink(filter) == CUCKOO_ERR);
    assert(filter->ntables == 2);
    assert(filter->tables[0].nbuckets == nb0 && filter->tables[1].nbuckets == nb1);
    assert(filter->tables[0].ntags == n0 && filter->tables[1].ntags == n1);
    assert(filter->tables[0].victim.used == v0);
    assert(filter->tables[1].victim.used == v1);
    assert(cuckoo_filter_used_memory(filter) == mem);

    /* 数据完好:回滚没有吞掉任何 tag。 */
    assert(cuckoo_filter_contains(filter, key, klen) == CUCKOO_OK);
    for (int i = 0; i < CUCKOO_FILTER_TAGS_PER_BUCKET * 2 + 2; i++)
        assert(cuckoo_filter_delete(filter, key, klen) == CUCKOO_OK);
    assert(cuckoo_filter_contains(filter, key, klen) == CUCKOO_ERR);

    cuckoo_filter_free(filter);
    return 1;
}

/* -----------------------------------------------------------------------------
 * 全部删空后:摘掉空表,但必须至少保留一张。
 *
 * 一次调用只摘一张表,所以 4 张表要摘 3 次,然后停在基表上:
 * 基表虽然同样是空的,但它的尺寸是调用方通过 estimated_keys 声明的容量,
 * shrink 不碰,于是后续调用一律返回 CUCKOO_ERR。
 *
 * 这个测试也顺带覆盖了前面测出来的"内存只增不减"问题:
 * 涨到 4 张表时占 87040 字节,全部删空后 delete 一个字节都不还,
 * shrink 能把它降到只剩基表的 1024 字节 —— 这就是缩容能达到的下限。
 * -------------------------------------------------------------------------- */
int test_cuckoo_shrink_drops_empty_tables() {
    char buf[32];

    cuckoo_filter_t *filter = cuckoo_filter_new(cuckoo_gen_hash_function,
            CUCKOO_FILTER_BITS_PER_TAG_8, 1000);
    assert(filter != NULL);
    size_t base_nbuckets = filter->tables[0].nbuckets;

    /* 用互不相同的 key 一直插到表数达到上限。 */
    long n = 0;
    while (filter->ntables < CUCKOO_FILTER_MAX_TABLES) {
        int len = snprintf(buf, sizeof(buf), "y:%ld", n);
        if (cuckoo_filter_insert(filter, buf, len) != CUCKOO_OK) break;
        n++;
    }
    assert(filter->ntables == CUCKOO_FILTER_MAX_TABLES);
    size_t mem_full = cuckoo_filter_used_memory(filter);
    assert(mem_full > 0);

    /* 每个 key 删一次,应该刚好清空(每次 delete 移除恰好一个 tag)。 */
    for (long i = 0; i < n; i++) {
        int len = snprintf(buf, sizeof(buf), "y:%ld", i);
        assert(cuckoo_filter_delete(filter, buf, len) == CUCKOO_OK);
    }
    for (int i = 0; i < filter->ntables; i++) {
        assert(filter->tables[i].ntags == 0);
        assert(filter->tables[i].victim.used == 0);
    }
    /* 删空了,但内存一点没还。 */
    assert(cuckoo_filter_used_memory(filter) == mem_full);

    /* ⚠️ 一次调用只处理一张表(分支 A:末表为空 → 直接摘)。
     * 所以从 4 张表缩到 1 张需要恰好 3 次调用,每次 ntables 减 1。 */
    int expect_ntables = CUCKOO_FILTER_MAX_TABLES;
    while (expect_ntables > 1) {
        size_t before = cuckoo_filter_used_memory(filter);
        assert(cuckoo_filter_shrink(filter) == CUCKOO_OK);
        expect_ntables--;
        assert(cuckoo_filter_used_memory(filter) < before);
        assert(filter->ntables == expect_ntables);
    }
    assert(filter->ntables == 1);

    /* ⚠️ 到基表就停。基表虽然也是空的,但它的尺寸是调用方声明的容量,
     * 不能碰,所以反复调用只会返回 ERR。 */
    size_t base_mem = cuckoo_filter_used_memory(filter);
    for (int i = 0; i < 3; i++) {
        assert(cuckoo_filter_shrink(filter) == CUCKOO_ERR);
        assert(filter->ntables == 1);
        assert(filter->tables[0].nbuckets == base_nbuckets);   /* 尺寸原样 */
        assert(cuckoo_filter_used_memory(filter) == base_mem);
    }
    /* 内存回到只剩基表的水位 —— 这就是 shrink 能达到的下限。 */
    assert(base_mem < mem_full);

    /* 摘干净之后仍然能正常工作。 */
    assert(cuckoo_filter_insert(filter, "revive", 6) == CUCKOO_OK);
    assert(cuckoo_filter_contains(filter, "revive", 6) == CUCKOO_OK);

    cuckoo_filter_free(filter);
    return 1;
}

/* -----------------------------------------------------------------------------
 * 缩容的核心语义:把末表并入前一张,严格作为 expand 的逆操作。
 *
 * 这个测试盯住三件事:
 *
 *   ① 一次调用只摘一张表,ntables 每次减 1。
 *   ② 等比阶梯 1:4:16:64 全程守住。合并末表之后前面几张表的尺寸不变,
 *      所以数组始终是 [nb0][4nb0][16nb0]... 这一点是"可逆"的前提。
 *   ③ 可逆:缩完之后继续写入,expand 会精确重建刚刚摘掉的那张表,
 *      尺寸序列和缩容前一模一样。
 *
 * ③ 是判断这个设计对不对的关键。如果改成"每张表各自就地瘦身",
 * 阶梯会被打乱,后续 expand 从一个奇怪的基数起算 4 倍,
 * 连"总容量还剩多少"都推算不出来。
 *
 * 全程还要校验末表的 tag 一个都没丢 —— 合并是搬运,不是丢弃。
 * -------------------------------------------------------------------------- */
int test_cuckoo_shrink_merges_last_table_into_previous() {
    char buf[32];

    cuckoo_filter_t *filter = cuckoo_filter_new(cuckoo_gen_hash_function,
            CUCKOO_FILTER_BITS_PER_TAG_8, 1000);
    assert(filter != NULL);
    size_t nb0 = filter->tables[0].nbuckets;
    assert(nb0 == 256);

    /* 涨到 4 张表。 */
    long n = 0;
    while (filter->ntables < CUCKOO_FILTER_MAX_TABLES) {
        int len = snprintf(buf, sizeof(buf), "m:%ld", n);
        if (cuckoo_filter_insert(filter, buf, len) != CUCKOO_OK) break;
        n++;
    }
    assert(filter->ntables == CUCKOO_FILTER_MAX_TABLES);

    /* 阶梯必须是严格的 4 倍等比。 */
    for (int i = 0; i < filter->ntables; i++)
        assert(filter->tables[i].nbuckets ==
                nb0 * (size_t)(1 << (2 * i)));       /* nb0 × 4^i */

    /* 删掉 95%,腾出合并所需的空间。 */
    long keep = n / 20;
    for (long i = keep; i < n; i++) {
        int len = snprintf(buf, sizeof(buf), "m:%ld", i);
        assert(cuckoo_filter_delete(filter, buf, len) == CUCKOO_OK);
    }

    /* 逐步缩容。每一步都校验:表数减 1、tag 总数不变、阶梯仍是等比。 */
    size_t total_before = 0;
    for (int i = 0; i < filter->ntables; i++) total_before += filter->tables[i].ntags;

    int steps = 0;
    while (filter->ntables > 1) {
        int prev_ntables = filter->ntables;
        if (cuckoo_filter_shrink(filter) != CUCKOO_OK) break;   /* 装不下就停 */
        steps++;
        assert(filter->ntables == prev_ntables - 1);

        /* tag 一个都不能少 —— 合并是搬运,不是丢弃。 */
        size_t total_now = 0;
        for (int i = 0; i < filter->ntables; i++) total_now += filter->tables[i].ntags;
        assert(total_now == total_before);

        /* 阶梯仍然从 nb0 起、严格 4 倍等比。 */
        for (int i = 0; i < filter->ntables; i++)
            assert(filter->tables[i].nbuckets == nb0 * (size_t)(1 << (2 * i)));
    }
    assert(steps > 0);                       /* 至少摘掉了一张表 */
    assert(filter->ntables < CUCKOO_FILTER_MAX_TABLES);

    /* 剩下的 key 一个都不能丢。 */
    for (long i = 0; i < keep; i++) {
        int len = snprintf(buf, sizeof(buf), "m:%ld", i);
        assert(cuckoo_filter_contains(filter, buf, len) == CUCKOO_OK);
    }

    /* ③ 可逆性:继续写入,阶梯应当原样长回 4 张表且尺寸序列不变。 */
    long r = 0;
    while (filter->ntables < CUCKOO_FILTER_MAX_TABLES) {
        int len = snprintf(buf, sizeof(buf), "re:%ld", r);
        if (cuckoo_filter_insert(filter, buf, len) != CUCKOO_OK) break;
        r++;
    }
    assert(filter->ntables == CUCKOO_FILTER_MAX_TABLES);
    for (int i = 0; i < filter->ntables; i++)
        assert(filter->tables[i].nbuckets == nb0 * (size_t)(1 << (2 * i)));

    cuckoo_filter_free(filter);
    return 1;
}

/* -----------------------------------------------------------------------------
 * 数据太密时必须拒绝缩容,并且什么都不改。
 *
 * 前表只有末表 1/4 的槽位,所以"末表并入前表"会把负载率放大到
 * (1 + CUCKOO_FILTER_BUCKETS_EXPANSION) = 5 倍,只在整体足够稀疏时才成立。
 * 装不下时的正确行为是原地返回 CUCKOO_ERR —— 不是尽力搬一部分,那会丢 tag。
 *
 * 只有一道预检,而且是 O(1) 的"物理上放不下"判断,不是策略阈值:
 *
 *     merged_ntags > 前表槽位数  →  直接拒绝
 *
 * 这里把两张表都填到较满,让 tag 总数超过前表槽位数,然后校验
 * ntables、各表尺寸、ntags、以及全部 key 都完好无损。
 * -------------------------------------------------------------------------- */
int test_cuckoo_shrink_refuses_when_too_dense() {
    char buf[32];

    cuckoo_filter_t *filter = cuckoo_filter_new(cuckoo_gen_hash_function,
            CUCKOO_FILTER_BITS_PER_TAG_8, 1000);
    assert(filter != NULL);

    /* 一直插到出现第 2 张表,再多插一些让末表也有相当数量的 tag。 */
    long n = 0;
    while (filter->ntables < 2) {
        int len = snprintf(buf, sizeof(buf), "p:%ld", n);
        if (cuckoo_filter_insert(filter, buf, len) != CUCKOO_OK) break;
        n++;
    }
    assert(filter->ntables == 2);
    for (long i = 0; i < 2000; i++) {
        int len = snprintf(buf, sizeof(buf), "q:%ld", i);
        assert(cuckoo_filter_insert(filter, buf, len) == CUCKOO_OK);
    }
    assert(filter->ntables == 2);

    /* 两张表的 tag 总数已经超过前表的槽位数 —— 物理上放不下,O(1) 预检就能判出来。 */
    size_t dst_slots = filter->tables[0].nbuckets * CUCKOO_FILTER_TAGS_PER_BUCKET;
    size_t merged = filter->tables[0].ntags + filter->tables[1].ntags;
    assert(merged > dst_slots);

    size_t mem = cuckoo_filter_used_memory(filter);
    size_t nb0 = filter->tables[0].nbuckets, nb1 = filter->tables[1].nbuckets;
    size_t n0 = filter->tables[0].ntags, n1 = filter->tables[1].ntags;

    /* 被拒绝:返回 CUCKOO_ERR,且一个字节都没动。 */
    assert(cuckoo_filter_shrink(filter) == CUCKOO_ERR);
    assert(filter->ntables == 2);
    assert(filter->tables[0].nbuckets == nb0 && filter->tables[1].nbuckets == nb1);
    assert(filter->tables[0].ntags == n0 && filter->tables[1].ntags == n1);
    assert(cuckoo_filter_used_memory(filter) == mem);

    /* 所有 key 完好。 */
    for (long i = 0; i < n; i++) {
        int len = snprintf(buf, sizeof(buf), "p:%ld", i);
        assert(cuckoo_filter_contains(filter, buf, len) == CUCKOO_OK);
    }
    for (long i = 0; i < 2000; i++) {
        int len = snprintf(buf, sizeof(buf), "q:%ld", i);
        assert(cuckoo_filter_contains(filter, buf, len) == CUCKOO_OK);
    }

    cuckoo_filter_free(filter);
    return 1;
}

/* -----------------------------------------------------------------------------
 * 合并时 victim cache 里的 tag 也必须跟着搬。
 *
 * victim 存的是一份真实数据(踢了 500 轮没安置下来的那个 tag),
 * 迁移时漏掉它就是静默丢数据 → false negative。它的 index 就是当初踢出链
 * 停下来的桶号,截断规则和普通槽完全一样。
 *
 * 构造一个"前表带着 victim、末表非空"的局面,好让分支 B 真的跑起来:
 *
 *   ① 同一个 key V 连插 9 次 → 基表里 V 占满 8 个候选槽,第 9 个进 victim。
 *      这里刻意用重复插入,只是为了低成本造出"victim 被占用",
 *      是对迁移路径的定向覆盖,不代表推荐用法。
 *   ② 用互不相同的 key 把基表顶到扩容 → 出现末表。
 *   ③ 把这些 key 全删掉 → 基表重新变稀疏,但 V 的 victim 依然占用
 *      (它的两个候选桶被自己的 8 个 tag 占满,try_eliminate 安置不回去)。
 *   ④ 再插几十个新 key → 末表非空,于是 shrink 走分支 B 而不是分支 A。
 *
 * 合并后必须同时成立:
 *   - victim.used 仍是 1(那个 tag 在新表里同样安置不下,又回到 victim)
 *   - contains(V) 仍是 OK
 *   - V 恰好还能被删 9 次 —— 这一条最关键,它说明 victim.index 被正确重算过,
 *     否则 delete 里的 "i1 == victim.index || i2 == victim.index" 会对不上,
 *     那个 tag 就变成删不掉的幽灵。
 * -------------------------------------------------------------------------- */
int test_cuckoo_shrink_migrates_victim() {
    const char *key = "victim-holder";
    size_t klen = strlen(key);
    const int rep = CUCKOO_FILTER_TAGS_PER_BUCKET * 2 + 1;   /* 9 */
    const long extra = 40;
    char buf[32];

    cuckoo_filter_t *filter = cuckoo_filter_new(cuckoo_gen_hash_function,
            CUCKOO_FILTER_BITS_PER_TAG_8, 1000);
    assert(filter != NULL);

    /* ① victim 造在基表里 */
    for (int i = 0; i < rep; i++)
        assert(cuckoo_filter_insert(filter, key, klen) == CUCKOO_OK);
    assert(filter->ntables == 1);
    assert(filter->tables[0].victim.used == 1);
    assert(filter->tables[0].ntags == (size_t)rep);

    /* ② 用不同 key 把基表顶到扩容 */
    long n = 0;
    while (filter->ntables < 2) {
        int len = snprintf(buf, sizeof(buf), "a:%ld", n);
        if (cuckoo_filter_insert(filter, buf, len) != CUCKOO_OK) break;
        n++;
    }
    assert(filter->ntables == 2);

    /* ③ 删掉这些 key,基表重新稀疏,但 V 的 victim 仍占用 */
    for (long i = 0; i < n; i++) {
        int len = snprintf(buf, sizeof(buf), "a:%ld", i);
        cuckoo_filter_delete(filter, buf, len);
    }
    assert(filter->tables[0].ntags == (size_t)rep);
    assert(filter->tables[0].victim.used == 1);

    /* ④ 让末表非空,确保走分支 B(合并)而不是分支 A(摘空表) */
    for (long i = 0; i < extra; i++) {
        int len = snprintf(buf, sizeof(buf), "b:%ld", i);
        assert(cuckoo_filter_insert(filter, buf, len) == CUCKOO_OK);
    }
    assert(filter->tables[1].ntags == (size_t)extra);
    size_t total = filter->tables[0].ntags + filter->tables[1].ntags;

    /* 合并 */
    assert(cuckoo_filter_shrink(filter) == CUCKOO_OK);
    assert(filter->ntables == 1);
    assert(filter->tables[0].ntags == total);          /* tag 一个没少 */

    /* victim 跟着搬过来了,而且在新表里同样安置不下,于是又进了 victim。 */
    assert(filter->tables[0].victim.used == 1);
    assert(cuckoo_filter_contains(filter, key, klen) == CUCKOO_OK);

    /* 一起被搬过来的普通 key 也都完好。 */
    for (long i = 0; i < extra; i++) {
        int len = snprintf(buf, sizeof(buf), "b:%ld", i);
        assert(cuckoo_filter_contains(filter, buf, len) == CUCKOO_OK);
    }

    /* ⚠️ 关键:V 恰好还能删 9 次 —— 证明 victim.index 被正确重算过。 */
    for (int i = 0; i < rep; i++)
        assert(cuckoo_filter_delete(filter, key, klen) == CUCKOO_OK);
    assert(filter->tables[0].victim.used == 0);
    assert(cuckoo_filter_contains(filter, key, klen) == CUCKOO_ERR);

    cuckoo_filter_free(filter);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("cuckoo function", 
            test_cuckoo() == 1);
        test_cond("cuckoo same key expand at 10th insert",
            test_cuckoo_same_key_expand() == 1);
        test_cond("cuckoo same key expand until max tables",
            test_cuckoo_same_key_expand_until_full() == 1);
        test_cond("cuckoo false deletion removes another key",
            test_cuckoo_false_deletion() == 1);
        test_cond("cuckoo false deletion is not idempotent",
            test_cuckoo_false_deletion_not_idempotent() == 1);
        test_cond("cuckoo cross table false deletion leaks old tag",
            test_cuckoo_cross_table_false_deletion() == 1);
        test_cond("cuckoo shrink never touches the base table",
            test_cuckoo_shrink_never_touches_base_table() == 1);
        test_cond("cuckoo shrink rolls back on migration failure",
            test_cuckoo_shrink_rolls_back_on_migration_failure() == 1);
        test_cond("cuckoo shrink drops empty tables keeping one",
            test_cuckoo_shrink_drops_empty_tables() == 1);
        test_cond("cuckoo shrink merges last table into previous",
            test_cuckoo_shrink_merges_last_table_into_previous() == 1);
        test_cond("cuckoo shrink refuses merge when too dense",
            test_cuckoo_shrink_refuses_when_too_dense() == 1);
        test_cond("cuckoo shrink migrates victim cache",
            test_cuckoo_shrink_migrates_victim() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}