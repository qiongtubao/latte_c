

#include "latte_counter.h"
static int sort_tag_by_gid(const void *a, const void *b) {
    const crdt_zset_tag *tag_a = *(crdt_zset_tag**)a, *tag_b = *(crdt_zset_tag**)b;
    /* We sort the vector clock unit by gid*/
    if (tag_a->gid > tag_b->gid)
        return 1;
    else if (tag_a->gid == tag_b->gid)
        return 0;
    else
        return -1;
}
#define add_tag(el, tag) {\
    if(tag == NULL) return el;\
    crdt_tag** tags = NULL;\
    if(el.len == 0) {\
        el.tags = tag;\
        return el;\
    } else if(el.len == 1) {\
        tags = zmalloc(sizeof(crdt_tag*) * 2);\
        long long a = el.tags;\
        tags[0] = (crdt_tag*)a;\
        tags[1] = tag;\
        el.len = 2;\
    } else {\
        tags = (crdt_tag**)el.tags;\
        tags = zealloc(tags, sizeof(crdt_tag*) * (el.len + 1));\
        tags[el.len] = tag;\
        el.len = el.len + 1;\
        return el;\
    }\
    el.tags = tags;\
    qsort(tags, el.len, sizeof(crdt_tag*), sort_tag_by_gid);\
    return el;\
}
element e1_add_tag(element el, crdt_tag* tag) {
    add_tag(el, tag);
}

element2 e2_add_tag(element2 el, crdt_tag* tag) {
    add_tag(el, tag);
}


