#include "cuckoo.h"
#include <assert.h>
#include "error/error.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>



int is_pow_of_2(uint64_t n) { return (n & (n - 1)) == 0 && n != 0; }
/*
    计算大于或等于 n 的最小的 2 的幂（即向上取整到最近的 2 的幂）
    输入数值 获得最小2^n的数
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

static size_t cuckoo_estimate_buckets(size_t estimated_keys) {
    size_t nbuckets = upper_pow_of_2(estimated_keys)/CUCKOO_FILTER_TAGS_PER_BUCKET; //2^n/4
    return nbuckets < CUCKOO_FILTER_TABLE_MIN_BUCKETS ? CUCKOO_FILTER_TABLE_MIN_BUCKETS : nbuckets; //[16,2^n/4]
}

static int bits_per_tag_array[CUCKOO_FILTER_BITS_PER_TAG_TYPES] = {8,12,16,32};
static inline int cuckoo_get_bits_per_tag(int bits_per_tag_type) {
    assert(bits_per_tag_type < CUCKOO_FILTER_BITS_PER_TAG_TYPES); //4种类型
    return bits_per_tag_array[bits_per_tag_type];
}
/**
    bits_per_tag 8
    nbuckets 8M
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



static uint8_t cuckoo_hash_function_seed[16] =
{15, 228, 29, 66, 3, 163, 118, 182, 101, 208, 229, 232, 2, 74, 115, 47};

uint64_t siphash(const uint8_t *in, const size_t inlen, const uint8_t *k);
uint64_t cuckoo_gen_hash_function(const void *key, int len) {
    return siphash(key,len,cuckoo_hash_function_seed);
}

/* 
    bits_per_tag_type 0
    estimated_keys  32000000
*/
static int is_littlen_endian() { int n = 1; return (*(char *)&n == 1); }
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

void cuckoo_table_deinit(cuckoo_table_t *table) {
    if (table->data) {
        zfree(table->data);
        table->data = NULL;
    }
}


void cuckoo_filter_free(cuckoo_filter_t* filter) {
    if (filter == NULL) return;
    for (int i = 0; i < filter->ntables; i++) {
        cuckoo_table_deinit(filter->tables + i);
    }
    zfree(filter->tables);
    zfree(filter);
}

static inline uint64_t cuckoo_filter_generate_hash(cuckoo_filter_t *filter,
        const char *key, size_t klen) {
    return filter->hash_fn(key,klen);
}

static inline void cuckoo_table_index_tag(cuckoo_table_t* table, uint64_t hv,
        size_t *i1, uint32_t *tag) {
    *i1 = (hv >> 32) & (table->nbuckets -1); // 取高 32 位  & (2^n - 1) =>  (hv >> 32) & (8M (2^23) -1)  (取后23位)
    *tag = (hv & 0xFFFFFFFF) & ((1ULL << table->bits_per_tag) - 1); // 取低 32位 & （1 << 8-1 )。（取后8位）
    *tag += *tag == 0;
}

/*
    为什么偏偏是 0x5bd1e995？
    这个十六进制数转换为十进制是 1540483477。它并不是随便选的，而是经过了严格的数学测试：
    奇数：它必须是一个奇数，这样才能保证乘法运算在二进制位上能够充分混合。
        大质数特性：在 32 位整数空间内，它与 
        2
        32
        2 
        32
    互质。这意味着当一个小范围的数字（比如 0~255 的指纹 tag）乘以它时，结果在 32 位空间内会均匀地跳跃分布，不会出现聚集现象。
    位模式：它的二进制表示为 0101 1011 1101 0001 1110 1001 1001 0101。你可以看到它的 1 和 0 分布非常均匀。当它与一个数字相乘时，能够最大程度地触发“雪崩效应”——即输入数字哪怕只改变 1 个比特，输出的结果也会有近一半的比特发生翻转。

*/
static inline size_t cuckoo_table_alt_index(cuckoo_table_t* table, size_t i1, uint32_t tag) {
    return (i1 ^ ((size_t) tag * 0x5bd1e995)) & (table->nbuckets - 1);
}

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

static inline cuckoo_table_t* cuckoo_filter_current_table(cuckoo_filter_t* filter) {
    return &filter->tables[filter->ntables-1];
}

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

int cuckoo_table_insert_kick_out(cuckoo_table_t* table, uint64_t hv) {
    size_t i;
    uint32_t tag;
    cuckoo_table_index_tag(table, hv, &i, &tag);
    return cuckoo_table_insert_kick_out_index_tag(table, i, tag);
}

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

static inline void cuckoo_table_try_eliminate_victim_cache(cuckoo_table_t* table) {
    if (table->victim.used) {
        table->victim.used = 0;
        table->ntags--;
        cuckoo_table_insert_kick_out_index_tag(table, table->victim.index, 
                table->victim.tag);
    }
}


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

size_t cuckoo_filter_used_memory(cuckoo_filter_t* filter) {
    size_t used_memory = 0;
    for (int i = 0; i < filter->ntables; i++) {
        cuckoo_table_t* table = filter->tables+i;
        used_memory += table->bytes_per_bucket * table->nbuckets;
    }
    return used_memory;
}

