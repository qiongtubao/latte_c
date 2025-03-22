#include "avl_set.h"
#include "sds/sds.h"


int avl_set_add(avl_set_t* set, void* key) {
    return avl_tree_put(set, key, NULL);
}

int avl_set_contains(avl_set_t* set, void* element) {
    avl_node_t* node = avl_tree_get(set, element);
    return node == NULL? 0 : 1;
}

int avl_set_type_add(set_t *set, void* key) {
    return avl_set_add((avl_tree_t *)set->data, key);
}

int avl_set_type_contains(set_t *set, void* key) {
    return avl_set_contains((avl_tree_t *)set->data, key);
}

int avl_set_type_remove(set_t* set, void* key) {
    return avl_tree_remove((avl_tree_t *)set->data, key);
}

size_t avl_set_type_size(set_t* set) {
    return avl_tree_size((avl_tree_t *)set->data);
}

void avl_set_type_delete(set_t* set) {
    avl_tree_delete((avl_tree_t *)set->data);
    zfree(set);
}

void avl_set_type_clear(set_t* set) {
    avl_tree_clear((avl_tree_t *)set->data);
}

latte_iterator_t* avl_set_type_get_iterator(set_t* set) {
    return avl_tree_get_iterator((avl_tree_t *)set->data);
}

struct set_func_t avl_set_func = {
    .add = avl_set_type_add,
    .contains = avl_set_type_contains,
    .remove = avl_set_type_remove,
    .size = avl_set_type_size,
    .release = avl_set_type_delete,
    .getIterator = avl_set_type_get_iterator,
    .clear = avl_set_type_clear
};
set_t* avl_set_new(avl_set_func_t* type) {
    set_t* s = zmalloc(sizeof(set_t));
    s->data = avl_tree_new(type);
    s->type = &avl_set_func;
    return s;
}
#define UNUSED(x) (void)x
avl_node_t* set_sds_node_new(void* key, void* value) {
    UNUSED(value);
    avl_node_t* node = zmalloc(sizeof(avl_node_t));
    node->left = NULL;
    node->right = NULL;
    node->key = sds_dup(key);
    node->height = 0;
    return node;
}


void UNSETVAL(avl_node_t* node, void* val) {
    UNUSED(node);
    UNUSED(val);
}

void set_sds_node_delete(avl_node_t* node) {
    sds_delete(node->key);
    zfree(node);
}

int set_sds_operator(void* a, void* b) {
    return sds_cmp((sds)a, (sds)b);
}

avl_set_func_t avl_set_sds_func = {
    .create_node = set_sds_node_new,
    .node_set_val = UNSETVAL,
    .operator = set_sds_operator,
    .release_node = set_sds_node_delete
};

