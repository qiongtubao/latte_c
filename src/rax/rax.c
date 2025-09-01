#include "rax.h"
#include "zmalloc/zmalloc.h"
#include "utils/utils.h"
#include <string.h>
/* 计算在一个大小为 'nodesize' 的节点中，其“字符部分”所需的填充字节数。
 * 填充的目的是为了保证存储在其中的子节点指针地址是内存对齐的。
 * 注意：我们在节点大小上加了4，因为节点本身包含一个4字节的头部。 */
#define rax_padding(nodesize) ((sizeof(void*)-(((nodesize)+4) % sizeof(void*))) & (sizeof(void*)-1))

rax_node_t* rax_new_node(int children, int data_field) {
    size_t node_size = sizeof(rax_node_t) + children + 
        rax_padding(children) +
        sizeof(rax_node_t*) * children;
    if (data_field) node_size += sizeof(void*);
    rax_node_t* node = (rax_node_t*)zmalloc(node_size);
    if (node == NULL) {
        return NULL;
    }
    node->is_key = 0;
    node->is_null = 0;
    node->is_compr = 0;
    node->size = children;
    return node;
}


rax_t* rax_new_with_meta(int meta_size) {
    rax_t* rax = (rax_t*)zmalloc(sizeof(rax_t) + meta_size);
    if (rax == NULL) {
        return NULL;
    }
    rax->num_ele = 0;
    rax->num_nodes = 1;
    rax->head = rax_new_node(0, 0);
    if (rax->head == NULL) {
        rax_delete(rax);
        return NULL;
    }    
    return rax;
}

rax_t* rax_new() {
    return rax_new_with_meta(0);
}


/* 计算节点当前的总大小。注意：第二行计算了在字符序列之后所需的填充字节，
 * 这是为了保证后续存储的子节点指针地址是内存对齐的。 */
#define rax_node_current_length(n) ( \
    sizeof(rax_node_t)+(n)->size+ \
    rax_padding((n)->size)+ \
    ((n)->is_compr ? sizeof(rax_node_t*) : sizeof(rax_node_t*)*(n)->size)+ \
    (((n)->is_key && !(n)->is_null)*sizeof(void*)) \
)

/* 返回指向节点中最后一个子节点指针的地址。
 * 对于压缩节点，这既是最后一个，也是唯一的子节点指针。 */
#define rax_node_last_child_ptr(n) ((rax_node_t**) ( \
    ((char*)(n)) + \
    rax_node_current_length(n) - \
    sizeof(rax_node_t*) - \
    (((n)->is_key && !(n)->is_null) ? sizeof(void*) : 0) \
))

/* Get the node auxiliary data. */
void *rax_get_data(rax_node_t *n) {
    if (n->is_null) return NULL;
    void **ndata =(void**)((char*)n+rax_node_current_length(n)-sizeof(void*));
    void *data;
    memcpy(&data,ndata,sizeof(data));
    return data;
}

void rax_recursive_delete(rax_t* rax, rax_node_t* node, void (*free_callback)(void*)) {
    // debug_node("free tracersing", node);
    int num_children = node->is_compr? 1: node->size;
    rax_node_t ** cp = rax_node_last_child_ptr(node);
    while (num_children--) {
        rax_node_t* child;
        memcpy(&child, cp, sizeof(child));
        rax_recursive_delete(rax, *cp, free_callback);
        cp--;
    }
    // debug_node("free depth-first", node);
    if (free_callback && node->is_key && node->is_null) {
        free_callback(rax_get_data(node));
    }
    zfree(node);
    rax->num_nodes--;
}

void raw_delete_with_callback(rax_t* rax, void (*free_callback)(void*)) {
    rax_recursive_delete(rax, rax->head, free_callback);
    latte_assert_with_info(rax->num_nodes == 0, "rax_delete_callback: num_nodes is not 0");
    zfree(rax);
}

void rax_delete(rax_t* rax) {
    raw_delete_with_callback(rax, NULL);
}