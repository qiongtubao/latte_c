
#include "../test/testhelp.h"
#include "../test/testassert.h"

#include "sds/sds.h"
#include "list.h"
#include "iterator/iterator.h"

void valueSdsFree(void* node) {
    sds_delete((sds)node);
}
int test_list_iterator() {
    list_t* l = list_new();
    l->free = valueSdsFree;
    list_add_node_tail(l, sds_new("hello"));
    list_add_node_tail(l, sds_new("world"));
    Iterator* iter = list_get_latte_iterator(l, 1);
    printf("???? : %p %p \n",iter->data, ((list_iterator_t*)(iter->data))->direction);
    int i = 0;
    while (iteratorHasNext(iter)) {
        iteratorNext(iter);
        i++;
    }
    assert(i == 2);
    iteratorRelease(iter);
    return 1;
}

int test_list() {
    list_t* l = list_new();
    
    list_add_node_head(l, (void*)0);
    list_add_node_tail(l, (void*)1L);
    list_add_node_tail(l, (void*)2L);
    list_add_node_tail(l, (void*)3L);
    Iterator* iter = list_get_latte_iterator(l, 0);
    long a[4] = {0L,1L,2L,3L};
    int i = 0;
    while (iteratorHasNext(iter)) {
        long v = (long)iteratorNext(iter);
        assert(a[i] == v);
        i++;
    }
    iteratorRelease(iter);

    iter = list_get_latte_iterator(l, 1);
    long b[4] = {3L,2L,1L,0L};
    i = 0;
    while (iteratorHasNext(iter)) {
        long v = (long)iteratorNext(iter);
        assert(b[i] == v);
        i++;
    }
    iteratorRelease(iter);

    long c[4] = {1L,0L,2L,3L};
    i = 0;
    list_move_head(l, l->head->next);
    for(int i = 0; i < 4; i++) {
        assert(list_node_value(list_index(l, i)) == c[i]);
    }

    long r = ((long)list_pop(l));
    assert(r == 1L);
    assert(list_length(l) == 3);

    list_delete(l);
    return 1;
}

int test_api(void) {

    {
        
        test_cond("list iterator test", 
            test_list_iterator() == 1);
        test_cond("list function test", 
            test_list() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}