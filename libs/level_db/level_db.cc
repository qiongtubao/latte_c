#include "leveldb/db.h" 
#include "leveldb/write_batch.h" 
#include "level_db.h"


void* level_db_open(sds dbname) {
    std::string db_path = dbname;
    leveldb::DB *ldbptr = nullptr;
    leveldb::Options options;
    leveldb::Status status = leveldb::DB::Open(options, db_path, &ldbptr);
    if(!status.ok()) {
        return NULL;
    }
    return ldbptr;
}

void* level_db_close(void* db) {

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
    leveldb::DB *ldbptr = (leveldb::DB *)db;
    leveldb::WriteOptions putOptions;
    putOptions.sync = true;
    leveldb::Status status = ldbptr->Put(putOptions, key, value);
    if(!status.ok()) {
        return 0;
    }
    return 1;
}