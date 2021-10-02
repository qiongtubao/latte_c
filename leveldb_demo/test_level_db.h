#ifdef __cplusplus
extern "C" {
#endif 
    #include "sds.h"
    void* level_db_open(sds dbname);
    void level_db_close(void* db) ;
    sds level_db_get(void* db, sds key);
    int level_db_set(void* db, sds key, sds value);
#ifdef __cplusplus
}
#endif 
