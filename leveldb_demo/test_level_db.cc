#include "test_level_db.h"
#include "leveldb/db.h" 
#include "leveldb/write_batch.h" 
#include <iostream>

void* level_db_open(sds dbname) {
    leveldb::DB *ldbptr = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;
    std::string db_path = dbname;
    leveldb::Status status = leveldb::DB::Open(options, db_path, &ldbptr);
    if(!status.ok()) {
        std::cerr << "open level db error:" << status.ToString() << std::endl;
        return NULL;
    }
    return ldbptr;
}

void level_db_close(void* db) {
    leveldb::DB *ldbptr = (leveldb::DB *)db;
    delete(ldbptr);
}

sds level_db_get(void* db, sds key) {
    leveldb::DB *ldbptr = (leveldb::DB *)db;
    leveldb::ReadOptions getOptions;
    std::string rdVal;
    leveldb::Status status = ldbptr->Get(getOptions, key, &rdVal);
    if (!status.ok()) {
        return NULL;
    }
    return sdsnewlen(rdVal.data(), rdVal.length());
}
int level_db_set(void* db, sds key, sds value) {
    leveldb::DB *ldbptr  = (leveldb::DB *)db;
    const std::string k = key;
    const std::string v = value;
    leveldb::WriteOptions putOptions;
    putOptions.sync = true;
    leveldb::Status status = ldbptr->Put(putOptions, k, v);
    if (!status.ok()) {
        std::cerr << "set key:"<< k << "value:" << v << "data error:" << status.ToString() << std::endl;
        return 0;
    }
    return 1;
}