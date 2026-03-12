
#include <assert.h>
#include "listeners.h"
#include <string.h>

/** @brief 忽略未使用参数，避免编译警告 */
#define DICT_NOTUSED(V) ((void) V)

/**
 * @brief SDS 键的哈希函数，计算 SDS 字符串的哈希值
 * @param key SDS 字符串指针
 * @return uint64_t 哈希值
 */
uint64_t dict_sds_hash(const void *key) {
    return dict_gen_hash_function((unsigned char*)key, sds_len((char*)key));
}

/**
 * @brief 比较两个 SDS 键是否相等
 * @param privdata 未使用的私有数据
 * @param key1     第一个 SDS 键
 * @param key2     第二个 SDS 键
 * @return int 相等返回 1，不等返回 0
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
 * @brief SDS 键的析构函数，释放 SDS 字符串内存
 * @param privdata 未使用的私有数据
 * @param val      要释放的 SDS 指针
 */
void dict_sds_destructor(void *privdata, void *val)
{
    DICT_NOTUSED(privdata);
    sds_delete(val);
}

/** @brief 用于键->监听节点字典的函数表，定义哈希/比较/析构行为 */
dict_func_t requestListenersDictType = {
    dict_sds_hash,          /* 哈希函数：对 SDS 键计算哈希 */
    NULL,                   /* key dup：不复制键 */
    NULL,                   /* val dup：不复制值 */
    dict_sds_key_compare,   /* key compare：按内容比较 SDS */
    dict_sds_destructor,    /* key destructor：释放 SDS 键 */
    NULL,                   /* val destructor：值由调用方管理 */
    NULL                    /* allow to expand：允许自动扩容 */
};

/**
 * @brief 创建通用监听节点（内部使用）
 * @param level  节点层级：REQUEST_LEVEL_SVR/DB/KEY
 * @param parent 父节点指针
 * @return requestListeners* 返回新建节点
 */
requestListeners *requestListenersCreate(int level,  requestListeners *parent) {
    requestListeners* listeners;
    listeners = zmalloc(sizeof(requestListeners));
    listeners->listeners = list_new(); /* 初始化监听器链表 */
    listeners->nlisteners = 0;         /* 初始计数为 0 */
    listeners->level = level;
    return listeners;
}

/**
 * @brief 创建数据库级别的监听节点
 * @param dbid   数据库编号
 * @param parent 父节点（服务器级别节点）
 * @return requestListeners* 返回新建 db 级节点
 */
requestListeners *dbRequestListenersCreate(redisDb dbid, requestListeners *parent) {
    requestListeners* listeners = requestListenersCreate(REQUEST_LEVEL_DB, parent);
    listeners->db.db = dbid;
    /* 创建键->键级监听节点的字典 */
    listeners->db.keys = dict_new(&requestListenersDictType);
    return listeners;
}

/**
 * @brief 创建键级别的监听节点，并注册到父 db 节点的字典中
 * @param key    监听的键对象
 * @param parent 父节点（db 级别节点）
 * @return requestListeners* 返回新建键级节点
 */
requestListeners *keyRequestListenersCreate(robj* key, requestListeners *parent) {
    requestListeners* listeners = requestListenersCreate(REQUEST_LEVEL_KEY, parent);
    listeners->key.key = key;
    /* 将键级节点注册到父 db 节点的字典中，以 key 为索引 */
    dict_add(parent->db.keys, key, listeners);
    return listeners;
}

/**
 * @brief 创建服务器级别的监听器根节点，并初始化所有 db 子节点
 * @param dbnum  数据库总数
 * @param parent 父节点，通常为 NULL
 * @return requestListeners* 返回新建服务器级根节点
 */
requestListeners *srvRequestListenersCreate(int dbnum, requestListeners *parent) {
    requestListeners* listeners = requestListenersCreate(REQUEST_LEVEL_SVR, parent);
    listeners->svr.dbnum = dbnum;
    /* 为每个 db 分配 db 级监听节点 */
    listeners->svr.dbs = zmalloc(dbnum * sizeof(requestListeners*));
    for(int i = 0; i < dbnum; i++) {
        listeners->svr.dbs[i] = dbRequestListenersCreate(i, listeners);
    }
    return listeners;
}

/**
 * @brief 在监听树中根据 db 和 key 查找对应节点
 *
 * 查找逻辑：
 * 1. db < 0 或服务器节点有监听器 → 返回服务器级节点
 * 2. key == NULL 或 db 节点有监听器 → 返回 db 级节点
 * 3. 否则查找键级节点，若不存在且 create=1 则新建
 *
 * @param root   服务器级根节点
 * @param db     数据库编号
 * @param key    键对象，NULL 表示只找到 db 级别
 * @param create 键级节点不存在时是否新建
 * @return requestListeners* 最终定位到的监听节点
 */
requestListeners* requestBindListeners(requestListeners *root, redisDb db, robj *key, int create) {
    requestListeners *svr_listeners, *db_listeners, *key_listeners;
    svr_listeners = root;
    /* db < 0 或服务器级别已有监听器时，直接返回服务器级节点 */
    if (db < 0 || list_length(svr_listeners->listeners)) {
        return svr_listeners;
    }

    db_listeners = svr_listeners->svr.dbs[db];
    /* key 为 NULL 或 db 级别已有监听器时，返回 db 级节点 */
    if (key == NULL || list_length(db_listeners->listeners)) {
        return db_listeners;
    }
    /* 在 db 节点的字典中查找键级监听节点 */
    key_listeners = (requestListeners*)dictFetchValue(db_listeners->db.keys, key);
    if (key_listeners == NULL) {
        if (create) {
            /* 不存在时新建键级监听节点 */
            key_listeners = keyRequestListenersCreate(key, db_listeners);
        }
    }
    return key_listeners;
}

/**
 * @brief 向上遍历监听树，对每个祖先节点的监听计数减一（内部使用）
 * @param listeners 起始节点
 */
static inline void requestListenersUnlink(requestListeners *listeners) {
    while (listeners) {
        listeners->nlisteners--;
        listeners = listeners->parent;
    }
}

/**
 * @brief 向监听节点推入一个监听器，并更新祖先节点的监听计数
 * @param listeners 目标监听节点（不能为 NULL）
 * @param listener  要推入的监听器指针
 */
void requestListenersPush(requestListeners *listeners,
        requestListener *listener) {
    serverAssert(listeners);
    /* 将监听器追加到当前节点的链表尾部 */
    list_add_node_tail(listeners->listeners, listener);
    /* 向上更新祖先节点的监听计数 */
    requestListenersLink(listeners);
}