
#ifndef __LATTE_ZSET_H
#define __LATTE_ZSET_H
#include "dict/dict.h"
#include "sds/sds.h"

#define ZSKIPLIST_MAXLEVEL 32 /* Should be enough for 2^64 elements */
#define ZSKIPLIST_P 0.25      /* Skiplist P = 1/4 */



typedef struct zskip_list_node_t {
    sds ele;
    double score;
    struct zskip_list_node_t *backward;
    struct zskip_list_level_t {
        struct zskip_list_node_t* forward;
        unsigned long span;
    } level[];
} zskip_list_node_t;

typedef struct zskip_list_t {
    struct zskip_list_node_t* header, *tail;
    unsigned long length;
    int level;
} zskip_list_t;

typedef struct zset_t {
    dict_t* dict;
    zskip_list_t *zsl;
} zset_t;


zskip_list_t* zskip_list_new();
zset_t* zset_new(dict_t* dict);
int zset_add(zset_t* zset, void* key, double score);
int zset_remove(zset_t* zset, void* key);

void zset_delete(zset_t* zset);

#endif