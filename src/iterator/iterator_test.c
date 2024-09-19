
#include "test/testhelp.h"
#include "test/testassert.h"
#include "iterator.h"
#include "zmalloc/zmalloc.h"

typedef struct arrayIterator {
    IteratorType* type;
    void* data;
    int len;
    int current_index;
    int one_size;
} arrayIterator;
bool arrayIteratorHasNext(Iterator* it) {
    arrayIterator* iterator = (arrayIterator*)it;
    return iterator->current_index < iterator->len;
}
void* arrayIteratorNext(Iterator *it) {
    arrayIterator* iterator = (arrayIterator*)it;
    iterator->current_index++;
    return iterator->data + (iterator->current_index-1)*iterator->one_size;
}

void arrayIteratorRelease(Iterator* it) {
    arrayIterator* iterator = (arrayIterator*)it;
    zfree(iterator);
}

IteratorType arrayIteratorType = {
    .hasNext = arrayIteratorHasNext,
    .next = arrayIteratorNext,
    .release = arrayIteratorRelease
};
Iterator* arrayIteratorCreate(void* data, int len, int one_size) {
    arrayIterator* iterator = zmalloc(sizeof(arrayIterator));
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
    Iterator* it = arrayIteratorCreate(array, 26, 1);
    for(int i = 0; i < 36; i++) {
        assert(iteratorHasNext(it) == true);
    }
    char* val = iteratorNext(it);
    assert(val[0] == 'a');
    val = iteratorNext(it);
    assert(val[0] == 'b');
    
    while(iteratorHasNext(it)) {
        val = iteratorNext(it);
        assert(val[0] > 'b');
    }

    iteratorRelease(it);

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