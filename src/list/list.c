/* adlist.c - A generic doubly linked list implementation
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdlib.h>
#include "list.h"
#include "zmalloc/zmalloc.h"
/* Create a new list. The created list can be freed with
 * list_delete(), but private value of every node need to be freed
 * by the user before to call list_delete(), or by setting a free method using
 * list_set_free_method.
 *
 * On error, NULL is returned. Otherwise the pointer to the new list. */
list_t *list_new(void)
{
    struct list_t *list;

    if ((list = zmalloc(sizeof(*list))) == NULL)
        return NULL;
    list->head = list->tail = NULL;
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;
    return list;
}

/* Remove all the elements from the list without destroying the list itself. */
void list_empty(list_t *list)
{
    unsigned long len;
    list_node_t *current, *next;

    current = list->head;
    len = list->len;
    while(len--) {
        next = current->next;
        if (list->free) list->free(current->value);
        zfree(current);
        current = next;
    }
    list->head = list->tail = NULL;
    list->len = 0;
}

/* Free the whole list.
 *
 * This function can't fail. */
void list_delete(list_t *list)
{
    list_empty(list);
    zfree(list);
}

/* Add a new node to the list, to head, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
list_t *list_add_node_head(list_t *list, void *value)
{
    list_node_t *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = value;
    if (list->len == 0) {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
    list->len++;
    return list;
}

/* Add a new node to the list, to tail, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
list_t *list_add_node_tail(list_t *list, void *value)
{
    list_node_t *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = value;
    if (list->len == 0) {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }
    list->len++;
    return list;
}

list_t *list_insert_node(list_t *list, list_node_t *old_node, void *value, int after) {
    list_node_t *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = value;
    if (after) {
        node->prev = old_node;
        node->next = old_node->next;
        if (list->tail == old_node) {
            list->tail = node;
        }
    } else {
        node->next = old_node;
        node->prev = old_node->prev;
        if (list->head == old_node) {
            list->head = node;
        }
    }
    if (node->prev != NULL) {
        node->prev->next = node;
    }
    if (node->next != NULL) {
        node->next->prev = node;
    }
    list->len++;
    return list;
}

/* Remove the specified node from the specified list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */
void list_del_node(list_t *list, list_node_t *node)
{
    if (node->prev)
        node->prev->next = node->next;
    else
        list->head = node->next;
    if (node->next)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;
    if (list->free) list->free(node->value);
    zfree(node);
    list->len--;
}

/* Returns a list iterator 'iter'. After the initialization every
 * call to list_next() will return the next element of the list.
 *
 * This function can't fail. */
list_iterator_t* list_get_iterator(list_t *list, int direction)
{
    list_iterator_t*iter;

    if ((iter = zmalloc(sizeof(*iter))) == NULL) return NULL;
    if (direction == AL_START_HEAD)
        iter->next = list->head;
    else
        iter->next = list->tail;
    iter->direction = direction;
    return iter;
}

/* Release the iterator memory */
void list_iterator_delete(list_iterator_t*iter) {
    zfree(iter);
}

/* Create an iterator in the list private iterator structure */
void list_rewind(list_t *list, list_iterator_t*li) {
    li->next = list->head;
    li->direction = AL_START_HEAD;
}

void list_rewind_tail(list_t *list, list_iterator_t*li) {
    li->next = list->tail;
    li->direction = AL_START_TAIL;
}

/* Return the next element of an iterator.
 * It's valid to remove the currently returned element using
 * list_del_node(), but not to remove other elements.
 *
 * The function returns a pointer to the next element of the list,
 * or NULL if there are no more elements, so the classical usage
 * pattern is:
 *
 * iter = list_get_iterator(list,<direction>);
 * while ((node = list_next(iter)) != NULL) {
 *     doSomethingWith(list_node_value(node));
 * }
 *
 * */
list_node_t *list_next(list_iterator_t*iter)
{
    list_node_t *current = iter->next;

    if (current != NULL) {
        if (iter->direction == AL_START_HEAD)
            iter->next = current->next;
        else
            iter->next = current->prev;
    }
    return current;
}

/* Duplicate the whole list. On out of memory NULL is returned.
 * On success a copy of the original list is returned.
 *
 * The 'Dup' method set with list_set_dup_method() function is used
 * to copy the node value. Otherwise the same pointer value of
 * the original node is used as value of the copied node.
 *
 * The original list both on success or error is never modified. */
list_t *list_dup(list_t *orig)
{
    list_t *copy;
    list_iterator_t iter;
    list_node_t *node;

    if ((copy = list_new()) == NULL)
        return NULL;
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;
    list_rewind(orig, &iter);
    while((node = list_next(&iter)) != NULL) {
        void *value;

        if (copy->dup) {
            value = copy->dup(node->value);
            if (value == NULL) {
                list_delete(copy);
                return NULL;
            }
        } else {
            value = node->value;
        }
        
        if (list_add_node_tail(copy, value) == NULL) {
            /* Free value if dup succeed but list_add_node_tail failed. */
            if (copy->free) copy->free(value);
            list_delete(copy);
            return NULL;
        }
    }
    return copy;
}

/* Search the list for a node matching a given key.
 * The match is performed using the 'match' method
 * set with list_set_match_method(). If no 'match' method
 * is set, the 'value' pointer of every node is directly
 * compared with the 'key' pointer.
 *
 * On success the first matching node pointer is returned
 * (search starts from head). If no matching node exists
 * NULL is returned. */
list_node_t *list_search_key(list_t *list, void *key)
{
    list_iterator_t iter;
    list_node_t *node;

    list_rewind(list, &iter);
    while((node = list_next(&iter)) != NULL) {
        if (list->match) {
            if (list->match(node->value, key)) {
                return node;
            }
        } else {
            if (key == node->value) {
                return node;
            }
        }
    }
    return NULL;
}

/* Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 the penultimate
 * and so on. If the index is out of range NULL is returned. */
list_node_t *list_index(list_t *list, long index) {
    list_node_t *n;

    if (index < 0) {
        index = (-index)-1;
        n = list->tail;
        while(index-- && n) n = n->prev;
    } else {
        n = list->head;
        while(index-- && n) n = n->next;
    }
    return n;
}

/* Rotate the list removing the tail node and inserting it to the head. */
void list_rotate_tail_to_head(list_t *list) {
    if (list_length(list) <= 1) return;

    /* Detach current tail */
    list_node_t *tail = list->tail;
    list->tail = tail->prev;
    list->tail->next = NULL;
    /* Move it as head */
    list->head->prev = tail;
    tail->prev = NULL;
    tail->next = list->head;
    list->head = tail;
}

/* Rotate the list removing the head node and inserting it to the tail. */
void list_rotate_head_to_tail(list_t *list) {
    if (list_length(list) <= 1) return;

    list_node_t *head = list->head;
    /* Detach current head */
    list->head = head->next;
    list->head->prev = NULL;
    /* Move it as tail */
    list->tail->next = head;
    head->next = NULL;
    head->prev = list->tail;
    list->tail = head;
}

/* Add all the elements of the list 'o' at the end of the
 * list 'l'. The list 'other' remains empty but otherwise valid. */
void list_join(list_t *l, list_t *o) {
    if (o->len == 0) return;

    o->head->prev = l->tail;

    if (l->tail)
        l->tail->next = o->head;
    else
        l->head = o->head;

    l->tail = o->tail;
    l->len += o->len;

    /* Setup other as an empty list. */
    o->head = o->tail = NULL;
    o->len = 0;
}


typedef struct latte_list_iterator_t {
    latte_iterator_t it;
    list_node_t* next;
    list_t* list;
} latte_list_iterator_t;

bool protected_latte_list_iterator_has_next(latte_iterator_t* iterator) {
    latte_list_iterator_t* it = (latte_list_iterator_t*)iterator;
    list_node_t* node = list_next(((list_iterator_t*)it->it.data));
    if (node == NULL) {
        return false;
    }
    it->next = node;
    return true;
}

void* protected_latte_list_iterator_next(latte_iterator_t* iterator) {
    latte_list_iterator_t* it = (latte_list_iterator_t*)iterator;
    list_node_t* node = it->next;
    it->next = NULL;
    return list_node_value(node);
}

void protected_latte_list_iterator_delete(latte_iterator_t* iterator) {
    latte_list_iterator_t* it = (latte_list_iterator_t*)iterator;
    list_iterator_delete(it->it.data);
    if (it->list != NULL) {
        list_delete(it->list);
        it->list = NULL;
    }
}

latte_iterator_func latte_list_iterator_func = {
    .has_next = protected_latte_list_iterator_has_next,
    .next = protected_latte_list_iterator_next,
    .release = protected_latte_list_iterator_delete
};

latte_iterator_t* list_get_latte_iterator(list_t* l, int opt) {
    latte_list_iterator_t* it = zmalloc(sizeof(latte_list_iterator_t));
    list_iterator_t* t;
    if (opt & LIST_ITERATOR_OPTION_TAIL) {
        t = list_get_iterator(l, AL_START_TAIL);
    } else {
        t = list_get_iterator(l, AL_START_HEAD);
    } 
    if (opt & LIST_ITERATOR_OPTION_DELETE_LIST) it->list = l;
    it->it.data = t;
    it->it.func = &latte_list_iterator_func;
    it->next = NULL;
    it->list = NULL;
    if (opt & LIST_ITERATOR_OPTION_DELETE_LIST) it->list = l;
    return (latte_iterator_t*)it;
}



void list_move_head(list_t* l, list_node_t* node) {
    if ( NULL == node->prev) {
        return;
    }
    node->prev->next = node->next;

    if (node->next != NULL) {
        node->next->prev = node->prev;
    } else {
        l->tail = node->prev;
    }

    node->prev = NULL;
    node->next = l->head;
    if (l->head != NULL) {
        l->head->prev = node;
    }
    l->head = node;
}

void* list_remove(list_t* l, list_node_t* node) {
    if (node == NULL) return NULL;
    void* result = list_node_value(node);
    if (l->free != NULL) {
        node->value = NULL;
    }
    list_del_node(l, node);
    return result;
}
void* list_lpop(list_t* l) {
    return list_remove(l, l->head);
}

void* list_rpop(list_t* l) {
    return list_remove(l, l->tail);
}