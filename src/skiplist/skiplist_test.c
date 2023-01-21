
#include "../test/testhelp.h"
#include "../test/testassert.h"
#include "skiplist.h"
int doubleComparator(sds a_ele, void* a_, sds b_ele, void* b_) {
    double a = *(double*)a_; 
    double b = *(double*)b_;
    if (a < b) {
        return -1;
    } else if (a > b) {
        return 1;
    } else {
        if (a_ele == b_ele) {
            return 0;
        } else if (a_ele == NULL) {
            return -1;
        } else if (b_ele == NULL) {
            return 1;
        } else {
            return sdscmp(a_ele, b_ele);
        }
    }

}
#define UNUSED(x) (void)(x)
void freeDouble(void* v) {
    UNUSED(v);
}
int test_skiplistNew() {
    skiplist* sl = skipListNew(&doubleComparator);
    assert(sl->length == 0);
    skipListFree(sl, &freeDouble);
    return 1;
}

int test_skiplistInsert() {
    skiplist* sl = skipListNew(&doubleComparator);
    sds a = sdsnew("a");
    double a_score  = 1.0;
    skipListInsert(sl, &a_score, a);
    assert(sl->length == 1);
    double min = 0.9;
    double max = 1.9;
    zrangespec range = {
        .min = &min,
        .max = &max,
        .maxex = 0,
        .minex = 0,
    };
    //1 point (1)
    //0.9 <= 1 <= 1.9 
    //0.9 <= 1 (first 1)
    //1.0 <= 1.9 (last 1)
    skiplistNode* first = skiplistFirstInRange(sl, &range);
    assert(first != NULL);
    assert(doubleComparator(NULL, first->score, NULL, &a_score) == 0);
    skiplistNode* last = skiplistLastInRange(sl, &range);
    assert(last != NULL);
    assert(doubleComparator(NULL, last->score, NULL, &a_score) == 0);

    //1 point (2)
    //0.1 <= 0.8 <= 1.0
    //0.1 <= 1.0 (frist NULL)
    //0.8 <= 1.0 (last NULL)
    min = 0.1;
    max = 0.8;
    range.min = &min;
    range.max = &max;
    first = skiplistFirstInRange(sl, &range);
    assert(first == NULL);
    // assert(doubleComparator(NULL, first->score, NULL, &a_score) == 0);
    last = skiplistLastInRange(sl, &range);
    assert(last == NULL);

    //1 point (3)
    // 1.0 <= 1.1 <= 2.1
    //1 <= 1.1 (frist NULL)
    //1 <= 2.1 (last NULL)
    min = 1.1;
    max = 2.1;
    range.min = &min;
    range.max = &max;
    first = skiplistFirstInRange(sl, &range);
    assert(first == NULL);
    last = skiplistLastInRange(sl, &range);
    assert(last == NULL);


    // test 2 point 
    sds b = sdsnew("b");
    double b_score  = 5.0;
    skipListInsert(sl, &b_score, b);
    assert(sl->length == 2);

    //2 point (1)
    //0.1 < 0.9 < 1 <  5
    min = 0.1;
    max = 0.9;
    range.min = &min;
    range.max = &max;
    first = skiplistFirstInRange(sl, &range);
    assert(first == NULL);

    // assert(doubleComparator(NULL, first->score, NULL, &b_score) == 0);
    last = skiplistLastInRange(sl, &range);
    assert(last == NULL);


    //2 point (2)
    //0.1 < 1 < 1.2 <  5

    min = 0.1;
    max = 1.2;
    range.min = &min;
    range.max = &max;
    first = skiplistFirstInRange(sl, &range);
    assert(first != NULL);
    assert(doubleComparator(NULL, first->score, NULL, &a_score) == 0);
    last = skiplistLastInRange(sl, &range);
    assert(last != NULL);
    assert(doubleComparator(NULL, last->score, NULL, &a_score) == 0);

    //2 point (3)
    //0.1 < 1  <  5 < 5.1

    min = 0.1;
    max = 5.2;
    range.min = &min;
    range.max = &max;
    first = skiplistFirstInRange(sl, &range);
    assert(first != NULL);
    assert(doubleComparator(NULL, first->score, NULL, &a_score) == 0);
    last = skiplistLastInRange(sl, &range);
    assert(last != NULL);
    assert(doubleComparator(NULL, last->score, NULL, &b_score) == 0);

    //2 point (4)
    //1  < 1.1 < 4.9 <  5
    min = 1.1;
    max = 4.9;
    range.min = &min;
    range.max = &max;
    first = skiplistFirstInRange(sl, &range);
    assert(first == NULL);
    // assert(doubleComparator(NULL, first->score, NULL, &b_score) == 0);
    last = skiplistLastInRange(sl, &range);
    assert(last == NULL);


    //2 point (5)
    //1  < 1.1  <  5 < 5.9
    min = 1.1;
    max = 5.9;
    range.min = &min;
    range.max = &max;
    first = skiplistFirstInRange(sl, &range);
    assert(first != NULL);
    assert(doubleComparator(NULL, first->score, NULL, &b_score) == 0);
    last = skiplistLastInRange(sl, &range);
    assert(last != NULL);
    assert(doubleComparator(NULL, last->score, NULL, &b_score) == 0);


    //2 point (5)
    //1  <  5 < 5.1 < 5.9
    min = 5.1;
    max = 5.9;
    range.min = &min;
    range.max = &max;
    first = skiplistFirstInRange(sl, &range);
    assert(first == NULL);
    last = skiplistLastInRange(sl, &range);
    assert(last == NULL);
    

    skipListFree(sl, &freeDouble);

    return 1;
}

int test_skiplistDel() {
    skiplist* sl = skipListNew(&doubleComparator);
    sds a = sdsnew("a");
    double a_score  = 1.0;
    sds b = sdsnew("b");
    double b_score  = 5.0;
    skipListInsert(sl, &a_score, a);
    assert(sl->length == 1);
    skipListDelete(sl, &a_score, a, NULL);
    assert(sl->length == 0);

    skipListInsert(sl, &a_score, a);
    skipListInsert(sl, &b_score, b);
    assert(sl->length == 2);
    skipListDelete(sl, &b_score, b, NULL);
    assert(sl->length == 1);

    skipListFree(sl, &freeDouble);
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("skiplistNew function", 
            test_skiplistNew() == 1);
        test_cond("skiplistInsert function",
            test_skiplistInsert() == 1);
        test_cond("skiplistDel function",
            test_skiplistDel() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}
