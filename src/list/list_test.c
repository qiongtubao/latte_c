
#include "../test/testhelp.h"
#include "../test/testassert.h"

#include "sds/sds.h"
#include "list.h"
#include "iterator/iterator.h"
#include "quicklist.h"

void valueSdsFree(void* node) {
    sds_delete((sds)node);
}
int test_list_iterator() {
    list_t* l = list_new();
    l->free = valueSdsFree;
    list_add_node_tail(l, sds_new("hello"));
    list_add_node_tail(l, sds_new("world"));
    latte_iterator_t* iter = list_get_latte_iterator(l, LIST_ITERATOR_OPTION_TAIL);
    int i = 0;
    while (latte_iterator_has_next(iter)) {
        latte_iterator_next(iter);
        i++;
    }
    assert(i == 2);
    latte_iterator_delete(iter);
    return 1;
}

int test_list() {
    list_t* l = list_new();
    
    list_add_node_head(l, (void*)0);
    list_add_node_tail(l, (void*)1L);
    list_add_node_tail(l, (void*)2L);
    list_add_node_tail(l, (void*)3L);
    latte_iterator_t* iter = list_get_latte_iterator(l, LIST_ITERATOR_OPTION_HEAD);
    long a[4] = {0L,1L,2L,3L};
    int i = 0;
    while (latte_iterator_has_next(iter)) {
        long v = (long)latte_iterator_next(iter);
        assert(a[i] == v);
        i++;
    }
    latte_iterator_delete(iter);

    iter = list_get_latte_iterator(l, LIST_ITERATOR_OPTION_TAIL);
    long b[4] = {3L,2L,1L,0L};
    i = 0;
    while (latte_iterator_has_next(iter)) {
        long v = (long)latte_iterator_next(iter);
        assert(b[i] == v);
        i++;
    }
    latte_iterator_delete(iter);

    long c[4] = {1L,0L,2L,3L};
    i = 0;
    list_move_head(l, l->head->next);
    for(int i = 0; i < 4; i++) {
        assert((long)list_node_value(list_index(l, i)) == c[i]);
    }

    long r = ((long)list_lpop(l));
    assert(r == 1L);
    assert(list_length(l) == 3);

    list_delete(l);
    return 1;
}


int test_quick_list() {
    struct quick_list_t* list = quick_list_create();
    quick_list_push_head(list, "hello", 6);
    quick_list_repr(list , 1);
    quick_list_release(list);
    return 1;
}

int test_api(void) {

    {
        
        test_cond("list iterator test", 
            test_list_iterator() == 1);
        test_cond("list function test", 
            test_list() == 1);
        test_cond("quick_list function", test_quick_list());
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}