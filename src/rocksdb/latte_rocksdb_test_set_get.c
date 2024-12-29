
#include "latte_rocksdb_test.h"
#include "latte_rocksdb.h"
#include "../test/testassert.h"

int test_rocksdb_set_get() {
    latte_rocksdb_t* db = latte_rocksdb_new();
    latte_rocksdb_family_t* family_info = latte_rocksdb_family_create(db, sds_new("default"));
    latte_error_t * error = latte_rocksdb_open(db, "set_get", 1, &family_info);
    assert(error_is_ok(error));
    latte_rocksdb_put_t* put = latte_rocksdb_put_new(family_info->cf_handle, sds_new("hello"), sds_new("world"));
    error = latte_rocksdb_write(db, 1, &put);
    assert(error_is_ok(error));
    latte_rocksdb_query_t* query = latte_rocksdb_query_new(family_info->cf_handle, sds_new("hello"));
    assert(0 == latte_rocksdb_read(db, 1, &query));
    assert(query->error == NULL);
    assert(strncmp(query->value, "world", 5) == 0);

    return 1;
}