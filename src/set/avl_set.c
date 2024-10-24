#include "avl_set.h"
#include "sds/sds.h"


int avl_set_add(avl_set_t* set, void* key) {
    return avlTreePut(set, key, NULL);
}

int avl_set_contains(avl_set_t* set, void* element) {
    avlNode* node = avlTreeGet(set, element);
    return node == NULL? 0 : 1;
}

int avl_set_type_add(set_t *set, void* key) {
    return avl_set_add(set->data, key);
}

int avl_set_type_contains(set_t *set, void* key) {
    return avl_set_contains(set->data, key);
}

int avl_set_type_remove(set_t* set, void* key) {
    return avl_set_remove(set->data, key);
}

size_t avl_set_type_size(set_t* set) {
    return avl_set_size(set->data);
}

void avl_set_type_delete(set_t* set) {
    avl_set_delete(set->data);
    zfree(set);
}

latte_iterator_t* avl_set_type_get_iterator(set_t* set) {
    return avl_set_get_iterator(set->data);
}

struct set_func_t avl_set_func = {
    .add = avl_set_type_add,
    .contains = avl_set_type_contains,
    .remove = avl_set_type_remove,
    .size = avl_set_type_size,
    .release = avl_set_type_delete,
    .getIterator = avl_set_type_get_iterator
};
set_t* avl_set_new(avl_set_func_t* type) {
    set_t* s = zmalloc(sizeof(set_t));
    s->data = avlTreeCreate(type);
    s->type = &avl_set_func;
    return s;
}
#define UNUSED(x) (void)x
avlNode* set_sds_node_new(void* key, void* value) {
    UNUSED(value);
    avlNode* node = zmalloc(sizeof(avlNode));
    node->left = NULL;
    node->right = NULL;
    node->key = sds_dup(key);
    node->height = 0;
    return node;
}


void UNSETVAL(avlNode* node, void* val) {
    UNUSED(node);
    UNUSED(val);
}

void set_sds_node_delete(avlNode* node) {
    sds_delete(node->key);
    zfree(node);
}

int set_sds_operator(void* a, void* b) {
    return sds_cmp((sds)a, (sds)b);
}

avl_set_func_t avl_set_sds_func = {
    .createNode = set_sds_node_new,
    .nodeSetVal = UNSETVAL,
    .operator = set_sds_operator,
    .releaseNode = set_sds_node_delete
};

