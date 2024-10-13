#ifndef __LATTE_LIST_H
#define __LATTE_LIST_H

#include "iterator/iterator.h"

typedef struct list_node_t {
    struct list_node_t *prev;
    struct list_node_t *next;
    void *value;
} list_node_t;

typedef struct list_iterator_t {
    list_node_t *next;
    int direction;
} list_iterator_t;

typedef struct list_t {
    list_node_t *head;
    list_node_t *tail;
    void *(*dup)(void *ptr);
    void (*free)(void *ptr);
    int (*match)(void *ptr, void *key);
    unsigned long len;
} list_t;


/* Functions implemented as macros */
#define list_length(l) ((l)->len)
#define list_first(l) ((l)->head)
#define list_last(l) ((l)->tail)
#define list_prev_node(n) ((n)->prev)
#define list_next_node(n) ((n)->next)
#define list_node_value(n) ((n)->value)

#define list_set_dup_method(l,m) ((l)->dup = (m))
#define list_set_free_method(l,m) ((l)->free = (m))
#define list_set_match_method(l,m) ((l)->match = (m))

#define list_get_dup_method(l) ((l)->dup)
#define list_get_free_method(l) ((l)->free)
#define list_get_match_method(l) ((l)->match)

/* Prototypes */
list_t *list_new(void);
void list_delete(list_t *list);
void list_empty(list_t *list);
list_t *list_add_node_head(list_t *list, void *value);
list_t *list_add_node_tail(list_t *list, void *value);
list_t *list_insert_node(list_t *list, list_node_t *old_node, void *value, int after);
void list_del_node(list_t *list, list_node_t *node);
list_iterator_t*list_get_iterator(list_t *list, int direction);
list_node_t *list_next(list_iterator_t*iter);
void list_delete_iterator(list_iterator_t*iter);
list_t *list_dup(list_t *orig);
list_node_t *list_search_key(list_t *list, void *key);
list_node_t *list_index(list_t *list, long index);
void list_rewind(list_t *list, list_iterator_t*li);
void list_rewind_tail(list_t *list, list_iterator_t*li);
void list_rotate_tail_to_head(list_t *list);
void list_rotate_head_to_tail(list_t *list);
void list_join(list_t *l, list_t *o);

Iterator* list_get_latte_iterator(list_t* l, int for_seq);
Iterator* list_get_latte_iterator_free_list(list_t* l, int for_seq);

void  list_move_head(list_t* l, list_node_t* node);
void* list_pop(list_t* l) ;
#define AL_START_HEAD 0
#define AL_START_TAIL 1
#endif
