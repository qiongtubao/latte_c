#include "zset.h"


zskip_list_node_t* zskip_list_node_new(int level, double score, sds ele) {
    zskip_list_node_t *zn =
        zmalloc(sizeof(*zn)+level*sizeof(struct zskip_list_level_t));
    zn->score = score;
    zn->ele = ele;
    return zn;
}

zskip_list_t* zskip_list_new() {
    int j;
    zskip_list_t* zsl;
    zsl = zmalloc(sizeof(*zsl));
    zsl->level = 1;
    zsl->length = 0;
    zsl->header = zskip_list_node_new(ZSKIPLIST_MAXLEVEL, 0, NULL);
    for (j = 0; j < ZSKIPLIST_MAXLEVEL; j++) {
        zsl->header->level[j].forward = NULL;
        zsl->header->level[j].span = 0;
    }
    zsl->header->backward = NULL;
    zsl->tail = NULL;
    return zsl;
}


zset_t* zset_new(dict_t* dict) {
    zset_t* z = zmalloc(sizeof(zset_t));
    z->zsl = zskip_list_new();
    z->dict = dict;
    return z;
}

//key is sds 
void zsl_free_node(zskip_list_node_t* node) {
    sds_delete(node->ele);
    zfree(node);
}

void zsl_free(zskip_list_t* zsl) {
    zskip_list_node_t* node = zsl->header->level[0].forward, *next;
    zfree(zsl->header);
    while(node) {
        next = node->level[0].forward;
        zsl_free_node(node);
        node = next;
    }
    zfree(zsl);
}

void zset_delete(zset_t* zs) {
    dict_delete(zs->dict);
    zsl_free(zs->zsl);
    zfree(zs);
}
