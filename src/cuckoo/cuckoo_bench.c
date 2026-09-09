/* =============================================================================
 * cuckoo filter 基准测试
 * =============================================================================
 *
 * 覆盖两类:
 *   ① 单点操作:insert / contains(命中) / contains(未命中) / delete / 替换
 *   ② 表合并:cuckoo_filter_shrink 的分支 B(末表并入前一张)
 *
 * ## 测量方法上的几个坑,都已规避
 *
 *   a) key 必须预生成。用 snprintf 在计时循环里造 key 要几十到上百 ns,
 *      而 filter 单点操作本身只有 30 ns 量级 —— 会被完全淹没。
 *      本文件把所有 key 预先摊平到一块定长 stride 的连续内存里。
 *
 *   b) 必须单独测哈希基线。每个操作都要先算一次 siphash,短 key 上
 *      siphash 的开销和整个 filter 操作是同一量级。不把它单列出来,
 *      就看不清时间到底花在哈希还是花在过滤器上。
 *
 *   c) 不要带 -fprofile-arcs / -ftest-coverage。覆盖率插桩会让结果失真几倍,
 *      所以 Makefile 里的 bench 目标用干净的 -O2,和 test 目标分开。
 *
 *   d) contains(未命中) 顺带统计真实假阳性率。理论值是
 *      1-(1-1/(2^f-1))^(8×ntables),实测能验证它,也能暴露表数带来的放大。
 *
 * ## 关于"改"
 *
 * cuckoo filter 没有原生 update:它只存指纹,同一个 key 的指纹不会变,
 * 没有"值"可改。业务上的"改"只能是"换一个 key",即 delete 旧的 + insert 新的。
 * 所以下面测的是这个组合,标注为 replace。
 *
 * ## 用法
 *
 *     make bench                 # 默认规模,几秒钟
 *     ./cuckoo_bench 5000000     # 指定 key 数
 *     ./cuckoo_bench 1000000 big # 额外跑 512MB+2GB 的大表合并(需要约 3.2GB 内存)
 * -------------------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include "cuckoo.h"
#include "zmalloc.h"   /* 手工搭 filter 时必须用 zmalloc,否则 cuckoo_filter_free 会 zfree 一块 malloc 的内存 */

/* 合并基准需要精确控制两张表的尺寸,公开 API 做不到(表尺寸由 estimated_keys
 * 和扩容路径决定)。这两个函数在 cuckoo.c 里是非 static 的,这里直接声明使用。
 * 属于测试代码对内部原语的定向使用,和 cuckoo_test.c 直接读 tables[i].ntags 同理。 */
void cuckoo_table_init(cuckoo_table_t *table, int bits_per_tag, size_t nbuckets);
void cuckoo_table_deinit(cuckoo_table_t *table);
int cuckoo_table_insert_index_tag(cuckoo_table_t *table, size_t i, uint32_t tag);

/* ---------------------------------------------------------------- 计时与随机 */

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* splitmix64。不用 rand():rand() 每次约 5-10 ns 且带全局锁语义,
 * 在 30 ns 量级的循环里是可观的干扰项。 */
static uint64_t rng_state = 0x9E3779B97F4A7C15ULL;
static inline uint64_t rng_next(void) {
    uint64_t z = (rng_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* ------------------------------------------------------------------ key 集合 */

/* 定长 stride 的连续内存,避免指针追逐带来的额外 cache miss。 */
#define KEY_STRIDE 24
typedef struct keyset {
    char  *buf;      /* n * KEY_STRIDE 字节 */
    size_t n;
    int    len;      /* 每个 key 的实际字节数,全部相同 */
} keyset;

static void keyset_init(keyset *ks, size_t n, const char *prefix) {
    ks->buf = malloc(n * KEY_STRIDE);
    ks->n = n;
    /* 定宽十进制,保证所有 key 等长 —— 长度不一会让 siphash 开销参差不齐。 */
    for (size_t i = 0; i < n; i++)
        ks->len = snprintf(ks->buf + i * KEY_STRIDE, KEY_STRIDE,
                           "%s%012zu", prefix, i);
}

static void keyset_free(keyset *ks) { free(ks->buf); ks->buf = NULL; }

static inline const char *keyset_at(const keyset *ks, size_t i) {
    return ks->buf + i * KEY_STRIDE;
}

/* ------------------------------------------------------------------ 结果输出 */

static void row(const char *name, size_t ops, double sec, const char *note) {
    printf("  %-26s %9.2f M ops/s   %7.1f ns/op   %8.3f s  %s\n",
           name, ops / sec / 1e6, sec / ops * 1e9, sec, note ? note : "");
}

/* 只读阶段跑多轮取最快的一轮。微基准里取 min 而不是平均,是因为噪声
 * (调度抢占、中断、频率波动)只会让某轮变慢、不会让它变快,
 * 所以最快那轮最接近"纯粹的操作成本"。
 * insert / delete / replace 是有状态的,跑一遍就改变了数据,没法这样重复,
 * 只能靠预热把缺页成本先摊掉(见 bench_ops 里的 warmup)。 */
#define READ_ROUNDS 3

static const char *tag_name(int type) {
    switch (type) {
        case CUCKOO_FILTER_BITS_PER_TAG_8:  return "8-bit";
        case CUCKOO_FILTER_BITS_PER_TAG_12: return "12-bit";
        case CUCKOO_FILTER_BITS_PER_TAG_16: return "16-bit";
        default:                            return "32-bit";
    }
}

static void dump_shape(cuckoo_filter_t *f) {
    cuckoo_filter_stat_t st;
    cuckoo_filter_get_stat(f, &st);
    printf("  形态: ntables=%zu  mem=%.2f MB  ntags=%zu  整体lf=%.3f  分表lf=",
           st.ntables, st.used_memory / 1048576.0, st.ntags, st.load_factor);
    for (size_t i = 0; i < st.ntables; i++) printf(" %.3f", st.load_factors[i]);
    printf("\n");
}

/* ============================================================================
 * ① 单点操作
 * ========================================================================== */
static void bench_ops(int tag_type, size_t n, size_t estimated_keys) {
    keyset live, absent;
    keyset_init(&live, n, "user:");
    keyset_init(&absent, n, "miss:");

    printf("\n--- %s tag | %zu 个 key | estimated_keys=%zu ---\n",
           tag_name(tag_type), n, estimated_keys);

    /* 基线:只算哈希,不碰过滤器。后面每一项都包含这一份开销。 */
    double t0 = now_sec();
    uint64_t sink = 0;
    for (size_t i = 0; i < n; i++)
        sink += cuckoo_gen_hash_function(keyset_at(&live, i), live.len);
    double t_hash = now_sec() - t0;
    row("baseline: siphash only", n, t_hash, "(每个操作都含这一份)");

    cuckoo_filter_t *f = cuckoo_filter_new(cuckoo_gen_hash_function,
            tag_type, estimated_keys);

    /* 预触碰初始表的物理页。
     *
     * zcalloc 拿到的是惰性零页,第一次写才真正分配物理页。不预热的话
     * 这几千次缺页会全部记在下面第一个计时阶段(insert)头上,
     * 实测能让 insert 看起来慢出 60%,而且随表大小无规律地波动。
     *
     * 为什么不用"插一遍再删一遍"来预热:那样会把 filter 提前撑到最终表数。
     * 而 cuckoo_filter_insert 是新→旧遍历、遇到第一个有空槽的表就写,
     * 最新表最大最空,几乎总是它赢 —— 于是所有数据都堆在末表,
     * 旧表全空。这个形态和"自然增长出来的"完全不同(实测分表负载率
     * 从 0.996/0.956/0.176 变成 0.000/0.005/0.475,FPP 从 6.4% 掉到 1.5%),
     * 测出来的查询成本和 FPP 都不具代表性。
     *
     * 所以这里只碰内存、不碰逻辑状态。扩容新建的表仍是冷的,
     * 但那部分开销本来就是"往估小了的 filter 里插入"的真实代价,应该计入。 */
    volatile uint64_t touch = 0;
    for (int t = 0; t < f->ntables; t++) {
        cuckoo_table_t *tb = &f->tables[t];
        size_t bytes = tb->bytes_per_bucket * tb->nbuckets;
        for (size_t off = 0; off < bytes; off += 4096) touch += tb->data[off];
    }

    /* insert */
    size_t ins_ok = 0;
    t0 = now_sec();
    for (size_t i = 0; i < n; i++)
        if (cuckoo_filter_insert(f, keyset_at(&live, i), live.len) == CUCKOO_OK)
            ins_ok++;
    double t_ins = now_sec() - t0;
    char note[80];
    if (ins_ok == n) note[0] = '\0';
    else snprintf(note, sizeof note, "只成功 %zu 个(已到容量上限)", ins_ok);
    row("insert", n, t_ins, note);
    printf("  → 扣掉哈希后,insert 净耗时 %.1f ns/op\n",
           (t_ins - t_hash) / n * 1e9);
    dump_shape(f);

    /* contains 命中(只读,取多轮最快) */
    size_t hit = 0;
    double t_hit = 1e9;
    for (int r = 0; r < READ_ROUNDS; r++) {
        hit = 0;
        t0 = now_sec();
        for (size_t i = 0; i < ins_ok; i++)
            if (cuckoo_filter_contains(f, keyset_at(&live, i), live.len) == CUCKOO_OK)
                hit++;
        double dt = now_sec() - t0;
        if (dt < t_hit) t_hit = dt;
    }
    snprintf(note, sizeof note, "命中 %zu/%zu%s", hit, ins_ok,
             hit == ins_ok ? " (无假阴性)" : "  <== 出现假阴性!");
    row("contains (命中)", ins_ok, t_hit, note);

    /* contains 未命中,顺带量真实 FPP */
    size_t fp = 0;
    double t_miss = 1e9;
    for (int r = 0; r < READ_ROUNDS; r++) {
        fp = 0;
        t0 = now_sec();
        for (size_t i = 0; i < n; i++)
            if (cuckoo_filter_contains(f, keyset_at(&absent, i), absent.len) == CUCKOO_OK)
                fp++;
        double dt = now_sec() - t0;
        if (dt < t_miss) t_miss = dt;
    }
    /* 理论值:每张表要扫 8 个候选槽,槽里有 tag 的概率约等于该表负载率,
     * 所以期望比对次数 ≈ 8 × Σ 各表负载率,命中任一即误判。 */
    cuckoo_filter_stat_t st;
    cuckoo_filter_get_stat(f, &st);
    double slots_probed = 0;
    for (size_t i = 0; i < st.ntables; i++)
        slots_probed += CUCKOO_FILTER_TAGS_PER_BUCKET * 2 * st.load_factors[i];
    double tag_values = (double)((1ULL << f->bits_per_tag) - 1);
    double fpp_theory = 1.0 - pow(1.0 - 1.0 / tag_values, slots_probed);
    snprintf(note, sizeof note, "FPP 实测 %.4f%% / 理论 %.4f%%",
             100.0 * fp / n, 100.0 * fpp_theory);
    row("contains (未命中)", n, t_miss, note);

    /* replace:cuckoo 没有原生 update,业务语义的"改"= 删旧 key + 插新 key。
     * 这里把 live[i] 换成 absent[i],做 n/2 次。 */
    size_t rep = n / 2, rep_ok = 0;
    t0 = now_sec();
    for (size_t i = 0; i < rep; i++) {
        if (cuckoo_filter_delete(f, keyset_at(&live, i), live.len) != CUCKOO_OK)
            continue;
        if (cuckoo_filter_insert(f, keyset_at(&absent, i), absent.len) == CUCKOO_OK)
            rep_ok++;
    }
    double t_rep = now_sec() - t0;
    snprintf(note, sizeof note, "成功 %zu/%zu (= delete + insert)", rep_ok, rep);
    row("replace (改)", rep, t_rep, note);

    /* delete 剩下的 */
    size_t del_ok = 0;
    t0 = now_sec();
    for (size_t i = rep; i < ins_ok; i++)
        if (cuckoo_filter_delete(f, keyset_at(&live, i), live.len) == CUCKOO_OK)
            del_ok++;
    double t_del = now_sec() - t0;
    size_t del_n = ins_ok > rep ? ins_ok - rep : 1;
    snprintf(note, sizeof note, "成功 %zu/%zu", del_ok, del_n);
    row("delete", del_n, t_del, note);
    printf("  → 扣掉哈希后,delete 净耗时 %.1f ns/op\n",
           (t_del - t_hash * del_n / n) / del_n * 1e9);
    dump_shape(f);

    cuckoo_filter_free(f);
    keyset_free(&live);
    keyset_free(&absent);
    (void)sink;
    (void)touch;
}

/* ============================================================================
 * ② 表合并 (cuckoo_filter_shrink 分支 B)
 * ==========================================================================
 *
 * 手工搭一个两表 filter,尺寸精确可控(公开 API 无法指定表尺寸)。
 * tag 按哈希随机落位,模拟真实数据分布。
 *
 * 观察点:
 *   - 合并耗时随源表尺寸线性增长(主路径是流式访问,不是随机访问瓶颈:
 *     目标桶 = 源桶 & mask,随源表顺序扫描而顺序推进;只有 alt 备选桶
 *     才是随机跳,且仅在首选桶已满时才会碰到)。
 *   - 合并后负载率越高,踢出链越长,每 tag 成本显著上升。
 *   - 负载率接近上限时可能白跑整趟迁移最后回滚返回 CUCKOO_ERR ——
 *     O(1) 硬预检只判"槽位数够不够",判不了"cuckoo 实际能否填到这么满"。
 */
static cuckoo_filter_t *build_two_tables(int tag_type, size_t dst_buckets) {
    cuckoo_filter_t *f = zmalloc(sizeof(cuckoo_filter_t));
    f->hash_fn = cuckoo_gen_hash_function;
    /* bits_per_tag 存的是真实位宽,不是枚举下标 */
    static const int bits[] = {8, 12, 16, 32};
    f->bits_per_tag = bits[tag_type];
    f->ntables = 2;
    f->tables = zmalloc(2 * sizeof(cuckoo_table_t));
    cuckoo_table_init(&f->tables[0], f->bits_per_tag, dst_buckets);
    cuckoo_table_init(&f->tables[1], f->bits_per_tag,
                      dst_buckets * CUCKOO_FILTER_BUCKETS_EXPANSION);
    return f;
}

static void fill_random(cuckoo_table_t *t, size_t want) {
    uint32_t mask = (uint32_t)((1ULL << t->bits_per_tag) - 1);
    for (size_t k = 0; k < want; k++) {
        uint64_t h = rng_next();
        size_t i = (h >> 32) & (t->nbuckets - 1);
        uint32_t tag = (uint32_t)(h & mask);
        tag += (tag == 0);
        if (cuckoo_table_insert_index_tag(t, i, tag) != CUCKOO_OK) break;
    }
}

static void bench_merge_one(int tag_type, size_t dst_buckets, double merged_lf) {
    size_t dst_slots = dst_buckets * CUCKOO_FILTER_TAGS_PER_BUCKET;
    size_t total = (size_t)(dst_slots * merged_lf);
    /* 两表槽位比 1:4,所以按 1/5 : 4/5 分配,模拟均匀分布 */
    size_t n_dst = total / (1 + CUCKOO_FILTER_BUCKETS_EXPANSION);

    cuckoo_filter_t *f = build_two_tables(tag_type, dst_buckets);
    fill_random(&f->tables[0], n_dst);
    fill_random(&f->tables[1], total - n_dst);

    size_t tags = f->tables[0].ntags + f->tables[1].ntags;
    size_t mem_before = cuckoo_filter_used_memory(f);
    size_t src_bytes = f->tables[1].bytes_per_bucket * f->tables[1].nbuckets;
    size_t dst_bytes = f->tables[0].bytes_per_bucket * f->tables[0].nbuckets;

    double t0 = now_sec();
    int rc = cuckoo_filter_shrink(f);
    double dt = now_sec() - t0;

    printf("  dst=%6.1f MB  src=%6.1f MB  合并后lf=%.2f  tag=%8.1f M  "
           "%s  %7.3f s  %6.1f ns/tag  释放 %.1f MB\n",
           dst_bytes / 1048576.0, src_bytes / 1048576.0, merged_lf, tags / 1e6,
           rc == CUCKOO_OK ? " OK" : "ERR", dt, dt / tags * 1e9,
           (mem_before - cuckoo_filter_used_memory(f)) / 1048576.0);

    cuckoo_filter_free(f);
}

/* 分支 A:末表为空,直接摘掉,零迁移。用来对比迁移成本。 */
static void bench_merge_empty(int tag_type, size_t dst_buckets) {
    cuckoo_filter_t *f = build_two_tables(tag_type, dst_buckets);
    fill_random(&f->tables[0], dst_buckets);           /* 只填前表,末表留空 */
    size_t mem_before = cuckoo_filter_used_memory(f);
    size_t dst_bytes = f->tables[0].bytes_per_bucket * f->tables[0].nbuckets;
    size_t src_bytes = f->tables[1].bytes_per_bucket * f->tables[1].nbuckets;
    double t0 = now_sec();
    int rc = cuckoo_filter_shrink(f);
    double dt = now_sec() - t0;
    printf("  dst=%6.1f MB  src=%6.1f MB  末表为空,零迁移直接摘掉        "
           "%s  %8.6f s                    释放 %.1f MB\n",
           dst_bytes / 1048576.0, src_bytes / 1048576.0,
           rc == CUCKOO_OK ? " OK" : "ERR", dt,
           (mem_before - cuckoo_filter_used_memory(f)) / 1048576.0);
    cuckoo_filter_free(f);
}

static void bench_merge(int big) {
    printf("\n=== ② 表合并 (cuckoo_filter_shrink) | 8-bit tag ===\n");
    bench_merge_empty(CUCKOO_FILTER_BITS_PER_TAG_8, 1ULL << 23);

    printf("\n  分支 B(末表并入前一张,需要全量迁移):\n");
    double lfs[] = {0.25, 0.50, 0.75, 0.90};
    /* 逐级 4 倍放大,用来确认耗时随尺寸线性增长 */
    size_t sizes[] = {1ULL << 21, 1ULL << 23, 1ULL << 25, 1ULL << 27};
    int nsz = big ? 4 : 3;      /* 默认最大 dst=128MB;big 模式到 512MB */
    for (int s = 0; s < nsz; s++) {
        for (int l = 0; l < 4; l++)
            bench_merge_one(CUCKOO_FILTER_BITS_PER_TAG_8, sizes[s], lfs[l]);
        printf("\n");
    }
    if (!big)
        printf("  (加 big 参数可跑 dst=512MB / src=2GB 的真实大表,需约 3.2GB 内存)\n");
}

/* ========================================================================== */

int main(int argc, char **argv) {
    size_t n = argc > 1 ? strtoul(argv[1], NULL, 10) : 1000000;
    int big = (argc > 2 && strcmp(argv[2], "big") == 0);
    if (n < 1000) n = 1000;

    printf("=== cuckoo filter bench ===\n");
    printf("每桶 %d 个槽 | 最多 %d 张表 | 扩容倍数 %d | 踢出上限 %d 轮\n",
           CUCKOO_FILTER_TAGS_PER_BUCKET, CUCKOO_FILTER_MAX_TABLES,
           CUCKOO_FILTER_BUCKETS_EXPANSION, CUCKOO_FILTER_MAX_ITERATION);

    printf("\n=== ① 单点操作 ===\n");
    /* 同一个 tag 位宽下的两种容量规划,对比"够用"和"要扩容"两种形态:
     *   estimated_keys = 4n  → 槽位充裕,单表,走 no_kick 快路径
     *   estimated_keys = n/8 → 必然多次扩容,查询要扫多张表,FPP 显著抬升 */
    for (int t = 0; t < CUCKOO_FILTER_BITS_PER_TAG_TYPES; t++)
        bench_ops(t, n, n * 4);

    printf("\n--- 对照:故意把 estimated_keys 估小,迫使扩容到多表 ---\n");
    bench_ops(CUCKOO_FILTER_BITS_PER_TAG_8, n, n / 8);

    bench_merge(big);
    return 0;
}
