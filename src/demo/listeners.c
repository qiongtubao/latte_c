
/**
 * @file listeners.c
 * @brief 请求监听器实现
 *        提供多级监听器管理（服务器级、数据库级、键级）和字典操作函数
 */
#include <assert.h>
#include "listeners.h"
#include <string.h>
#define DICT_NOTUSED(V) ((void) V)

/**
 * @brief SDS 字符串哈希函数
 * @param key SDS 字符串键
 * @return 哈希值
 */
uint64_t dict_sds_hash(const void *key) {
    return dict_gen_hash_function((unsigned char*)key, sds_len((char*)key));
}

/**
 * @brief SDS 字符串键比较函数
 * @param privdata 私有数据（未使用）
 * @param key1 第一个键
 * @param key2 第二个键
 * @return 1 表示相等，0 表示不相等
 */
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

/**
 * @brief SDS 字符串销毁函数
 * @param privdata 私有数据（未使用）
 * @param val 要销毁的值
 */
void dict_sds_destructor(void *privdata, void *val)
{
    DICT_NOTUSED(privdata);

    sds_delete(val);
}

/**< 请求监听器字典类型定义 */
dict_func_t requestListenersDictType = {
    dict_sds_hash,                    /**< 哈希函数 */
    NULL,                           /**< 键复制函数 */
    NULL,                           /**< 值复制函数 */
    dict_sds_key_compare,              /**< 键比较函数 */
    dict_sds_destructor,              /**< 键销毁函数 */
    NULL,                           /**< 值销毁函数 */
    NULL                            /**< 允许扩展函数 */
};



/**
 * @brief 创建请求监听器
 * @param level  监听器级别
 * @param parent 父监听器指针
 * @return 新创建的监听器指针
 */
requestListeners *requestListenersCreate(int level,  requestListeners *parent) {
    requestListeners* listeners;
    listeners = zmalloc(sizeof(requestListeners));
    listeners->listeners = listCreate();
    listeners->nlisteners = 0;
    listeners->level = level;
    return listeners;
}



/**
 * @brief 创建数据库级别的请求监听器
 * @param dbid   数据库ID
 * @param parent 父监听器指针
 * @return 新创建的数据库监听器指针
 */
requestListeners *dbRequestListenersCreate(redisDb dbid, requestListeners *parent) {
    requestListeners* listeners = requestListenersCreate(REQUEST_LEVEL_DB, parent);
    listeners->db.db = dbid;
    listeners->db.keys = dict_new(&requestListenersDictType);
    return listeners;
}

/**
 * @brief 创建键级别的请求监听器
 * @param key    Redis 对象键
 * @param parent 父监听器指针
 * @return 新创建的键监听器指针
 */
requestListeners *keyRequestListenersCreate(robj* key, requestListeners *parent) {
    requestListeners* listeners = requestListenersCreate(REQUEST_LEVEL_KEY, parent);
    // incrRefCount(key);
    listeners->key.key = key;
    dict_add(parent->db.keys, key, listeners);
    return listeners;
}

/**
 * @brief 创建服务器级别的请求监听器
 * @param dbnum  数据库数量
 * @param parent 父监听器指针
 * @return 新创建的服务器监听器指针
 */
requestListeners *srvRequestListenersCreate(int dbnum, requestListeners *parent) {
    requestListeners* listeners = requestListenersCreate(REQUEST_LEVEL_SVR,parent);
    listeners->svr.dbnum = dbnum;
    listeners->svr.dbs = zmalloc(dbnum* sizeof(requestListeners*));
    for(int i = 0; i < dbnum;i++) {
        listeners->svr.dbs[i] = dbRequestListenersCreate(i, listeners);
    }
    return listeners;
}

/**
 * @brief 绑定请求监听器到指定的数据库和键
 * @param root   根监听器
 * @param db     数据库ID
 * @param key    Redis 对象键
 * @param create 是否创建不存在的监听器
 * @return 绑定的监听器指针
 */
requestListeners* requestBindListeners(requestListeners *root, redisDb db, robj *key, int create) {
    requestListeners *svr_listeners, *db_listeners, *key_listeners;
    svr_listeners = root;
    if (db < 0 || listLength(svr_listeners->listeners)) {
        return svr_listeners;
    }

    db_listeners = svr_listeners->svr.dbs[db];
    if (key == NULL || listLength(db_listeners->listeners)) {
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

/**
 * @brief 取消链接请求监听器（减少监听器计数）
 * @param listeners 要取消链接的监听器
 */
static inline void requestListenersUnlink(requestListeners *listeners) {
    while (listeners) {
        listeners->nlisteners--;
        listeners = listeners->parent;
    }
}

/**
 * @brief 将监听器推送到监听器列表
 * @param listeners 监听器容器
 * @param listener  要添加的监听器
 */
void requestListenersPush(requestListeners *listeners,
        requestListener *listener) {
    serverAssert(listeners);
    listAddNodeTail(listeners->listeners, listener);
    requestListenersLink(listeners);
}