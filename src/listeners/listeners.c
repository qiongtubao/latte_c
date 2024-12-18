
#include <assert.h>
#include "listeners.h"
#include <string.h>
#define DICT_NOTUSED(V) ((void) V)
uint64_t dict_sds_hash(const void *key) {
    return dict_gen_hash_function((unsigned char*)key, sds_len((char*)key));
}

int dict_sds_key_compare(void *privdata, const void *key1,
        const void *key2)
{
    int l1,l2;
    DICT_NOTUSED(privdata);

    l1 = sds_len((sds)key1);
    l2 = sds_len((sds)key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

void dict_sds_destructor(void *privdata, void *val)
{
    DICT_NOTUSED(privdata);

    sds_delete(val);
}

dict_func_t requestListenersDictType = {
    dict_sds_hash,                    /* hash function */
    NULL,                           /* key dup */
    NULL,                           /* val dup */
    dict_sds_key_compare,              /* key compare */
    dict_sds_destructor,              /* key destructor */
    NULL,                           /* val destructor */
    NULL                            /* allow to expand */
};




requestListeners *requestListenersCreate(int level,  requestListeners *parent) {
    requestListeners* listeners;
    listeners = zmalloc(sizeof(requestListeners));
    listeners->listeners = list_new();
    listeners->nlisteners = 0;
    listeners->level = level;
    return listeners;
}



requestListeners *dbRequestListenersCreate(redisDb dbid, requestListeners *parent) {
    requestListeners* listeners = requestListenersCreate(REQUEST_LEVEL_DB, parent);
    listeners->db.db = dbid;
    listeners->db.keys = dict_new(&requestListenersDictType);
    return listeners;
}

requestListeners *keyRequestListenersCreate(robj* key, requestListeners *parent) {
    requestListeners* listeners = requestListenersCreate(REQUEST_LEVEL_KEY, parent);
    // incrRefCount(key);
    listeners->key.key = key;
    dict_add(parent->db.keys, key, listeners);
    return listeners;
}

requestListeners *srvRequestListenersCreate(int dbnum, requestListeners *parent) {
    requestListeners* listeners = requestListenersCreate(REQUEST_LEVEL_SVR,parent);
    listeners->svr.dbnum = dbnum;
    listeners->svr.dbs = zmalloc(dbnum* sizeof(requestListeners*));
    for(int i = 0; i < dbnum;i++) {
        listeners->svr.dbs[i] = dbRequestListenersCreate(i, listeners);
    }
    return listeners;
}

requestListeners* requestBindListeners(requestListeners *root, redisDb db, robj *key, int create) {
    requestListeners *svr_listeners, *db_listeners, *key_listeners;
    svr_listeners = root;
    if (db < 0 || list_length(svr_listeners->listeners)) {
        return svr_listeners;
    }

    db_listeners = svr_listeners->svr.dbs[db];
    if (key == NULL || list_length(db_listeners->listeners)) {
        return db_listeners;
    }
    key_listeners = (requestListeners*)dictFetchValue(db_listeners->db.keys,key);
    if (key_listeners == NULL) {
        if (create) {
            key_listeners = keyRequestListenersCreate(
                    key, db_listeners);
        }
    }
    return key_listeners;
}

static inline void requestListenersUnlink(requestListeners *listeners) {
    while (listeners) {
        listeners->nlisteners--;
        listeners = listeners->parent;
    }
}

void requestListenersPush(requestListeners *listeners,
        requestListener *listener) {
    serverAssert(listeners);
    list_add_node_tail(listeners->listeners, listener);
    requestListenersLink(listeners);
}