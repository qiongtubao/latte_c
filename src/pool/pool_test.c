#include "test/testhelp.h"
#include "test/testassert.h"
#include "mem.h"
#include "log/log.h"

int test_mem_pool() {
    mem_pool_t* pool = mem_pool_new(sds_new("test"));
    assert(mem_pool_init(pool, sizeof(long), false, 1, 2) == 0);
    assert(pool->size == 2);
    long r1 = (long)mem_pool_alloc(pool);
    long r2 = (long)mem_pool_alloc(pool);
    assert(NULL == mem_pool_alloc(pool));
    mem_pool_free(pool, (void*)r1);
    for(int i = 0; i< 100;i++) {
        long r3 = (long)mem_pool_alloc(pool);
        assert(r1 == r3);
        mem_pool_free(pool, (void*)r3);
    }
    assert(mem_pool_delete(pool) == 0);
    mem_pool_free(pool, (void*)r2);

    assert(mem_pool_delete(pool) == 1);
    return 1;
}

int test_api(void) {
    log_init();
    assert(log_add_stdout(LATTE_LIB, LOG_DEBUG) == 1);
    {
#ifdef LATTE_TEST
        // ..... private
#endif
        test_cond("test pool fuinction", 
            test_mem_pool() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}