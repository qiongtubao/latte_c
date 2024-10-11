#include "test/testhelp.h"
#include "test/testassert.h"
#include "bitmap.h"

// 测试位
bool bitmap_test(bitmap_t  bitmap, size_t index) {
    unsigned long *p = (unsigned long*)&(bitmap[index / 8]);
    return (*p & (1UL << (index % 8))) != 0;
}

// 打印位图
void bitmap_print(bitmap_t  bitmap) {
    for (size_t i = 0; i < bit_size(bitmap); i++) {
        printf("%d ", bitmap_test(bitmap, i));
    }
    printf("\n");
}
int test_bitmap() {
    bitmap_t  map = bitmap_new(50);
    
    bitmap_set(map, 10);
    assert( 10 == bitmap_next_setted(map, 0));
    assert( 9 == bitmap_next_unsetted(map, 9));
    assert( 11 == bitmap_next_unsetted(map, 10));

    

    bitmap_set(map, 20);
    bitmap_set(map, 30);

    bitmap_print(map);
    assert( 1  == bitmap_get(map, 10));
    bitmap_clear(map, 10);
    assert( 0 == bitmap_get(map, 10));


    printf("Bit 10 is set: %d\n", bitmap_test(map, 10));
    printf("Bit 20 is set: %d\n", bitmap_test(map, 20));
    printf("Bit 30 is set: %d\n", bitmap_test(map, 30));
    return 1;
}
int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("bitmap function", 
            test_bitmap() == 1);

        
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}