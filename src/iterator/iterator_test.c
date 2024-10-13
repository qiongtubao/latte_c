
#include "test/testhelp.h"
#include "test/testassert.h"
#include "iterator.h"
#include "zmalloc/zmalloc.h"

typedef struct array_iterator_t {
    latte_iterator_func* type;
    void* data;
    int len;
    int current_index;
    int one_size;
} array_iterator_t;

bool arrayIteratorHasNext(latte_iterator_t* it) {
    array_iterator_t* iterator = (array_iterator_t*)it;
    return iterator->current_index < iterator->len;
}
void* arrayIteratorNext(latte_iterator_t *it) {
    array_iterator_t* iterator = (array_iterator_t*)it;
    iterator->current_index++;
    return iterator->data + (iterator->current_index-1)*iterator->one_size;
}

void arrayIteratorRelease(latte_iterator_t* it) {
    array_iterator_t* iterator = (array_iterator_t*)it;
    zfree(iterator);
}

latte_iterator_func arrayIteratorType = {
    .hasNext = arrayIteratorHasNext,
    .next = arrayIteratorNext,
    .release = arrayIteratorRelease
};
latte_iterator_t* arrayIteratorCreate(void* data, int len, int one_size) {
    array_iterator_t* iterator = zmalloc(sizeof(array_iterator_t));
    iterator->type = &arrayIteratorType;
    iterator->data = data;
    iterator->len = len;
    iterator->current_index = 0;
    iterator->one_size = one_size;
    return iterator;
}
int test_array_iterator() {
    char array[26];
    for(int i = 0; i < 26; i++) {
        array[i] = 'a' + i;
    }
    latte_iterator_t* it = arrayIteratorCreate(array, 26, 1);
    for(int i = 0; i < 36; i++) {
        assert(latte_iterator_has_next(it) == true);
    }
    char* val = latte_iterator_next(it);
    assert(val[0] == 'a');
    val = latte_iterator_next(it);
    assert(val[0] == 'b');
    
    while(latte_iterator_has_next(it)) {
        val = latte_iterator_next(it);
        assert(val[0] > 'b');
    }

    latte_iterator_delete(it);

    return 1;
}
int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("array_iterator function", 
            test_array_iterator() == 1);
        
        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}