#ifndef __LATTE_RAX_H__
#define __LATTE_RAX_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "iterator/iterator.h"

typedef struct rax_node_t {
    uint32_t is_key:1;
    uint32_t is_null:1;
    uint32_t is_compr:1;
    uint32_t size:29;
    unsigned char data[];
} rax_node_t;

typedef struct rax_t {
    rax_node_t *node;
    uint64_t num_ele;
    uint64_t num_nodes;
    void *meta_data[];
} rax_t;

#define RAX_STACK_STATIC_SIZE 32
typedef struct rax_stack_t {
    void **stack;
    size_t items, max_items;
    void * static_item[RAX_STACK_STATIC_SIZE];
    int oom;
} rax_stack_t;

rax_t* rax_new();
rax_t* rax_new_with_meta(int meta_size);
void rax_delete(rax_t *rax);
int rax_insert(rax_t* rax, unsigned char *key, size_t len, void *data, void** old);
int rax_try_insert(rax_t* rax, unsigned char *key, size_t len, void *data, void** old);
int rax_remove(rax_t* rax, unsigned char *key, size_t len, void** data);
int rax_get(rax_t* rax, unsigned char *key, size_t len, void** data);

latte_iterator_t* rax_get_iterator(rax_t* rax);


#endif