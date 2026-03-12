/**
 * @file listeners.h
 * @brief 请求监听器（RequestListeners）演示接口
 *        实现多级（服务器/数据库/Key）请求监听器树结构，
 *        支持将监听器绑定到指定 db 或 key 的节点，并通过链表管理多个监听回调。
 *        该模块为演示用途，参考 Redis keyspace notification 设计。
 */

#include "../sds/sds.h"
#include "../dict/dict.h"
#include "../list/list.h"

/** @brief 数据库编号类型（演示用，实际为 int） */
typedef int redisDb;
/** @brief Redis 对象类型（演示用，实际为 char） */
typedef char robj;

/** @brief 监听器层级：服务器级（监听所有数据库） */
#define REQUEST_LEVEL_SVR 0
/** @brief 监听器层级：数据库级（监听指定 db 的所有 key） */
#define REQUEST_LEVEL_DB 1
/** @brief 监听器层级：Key 级（监听指定 key） */
#define REQUEST_LEVEL_KEY 2


/**
 * @brief 请求监听器树节点结构体
 *        以树形结构组织三级监听器（svr → db → key），
 *        每个节点维护当前级别的监听回调列表及子节点引用。
 */
typedef struct requestListeners {
    list *listeners;                  /**< 当前节点的监听回调链表 */
    struct requestListeners *parent;  /**< 父节点指针（NULL 表示根节点） */
    int nlisteners;                   /**< 当前节点及所有子节点的监听器总数 */
    int level;                        /**< 节点层级：REQUEST_LEVEL_SVR/DB/KEY */
    union {
        struct {
            int dbnum;                        /**< svr 级：数据库总数 */
            struct requestListeners **dbs;    /**< svr 级：各 db 节点指针数组 */
        } svr;
        struct {
            redisDb db;                       /**< db 级：数据库编号 */
            dict_t *keys;                     /**< db 级：key → requestListeners 的映射字典 */
        } db;
        struct {
            robj *key;                        /**< key 级：被监听的 key 对象 */
        } key;
    };
} requestListeners;

/**
 * @brief 在监听器树中绑定或查找指定 db/key 的监听器节点
 * @param root   根节点（svr 级）指针
 * @param db     目标数据库编号
 * @param key    目标 key 对象指针（NULL 表示 db 级）
 * @param create 不存在时是否自动创建：非零为创建，0 为仅查找
 * @return 找到或新建的 requestListeners 节点指针；失败返回 NULL
 */
requestListeners* requestBindListeners(requestListeners *root, redisDb db, robj *key, int create);

/**
 * @brief 创建服务器级（svr）监听器根节点
 * @param dbnum  数据库总数
 * @param parent 父节点指针（通常为 NULL）
 * @return 新建的 requestListeners 根节点指针；失败返回 NULL
 */
requestListeners *srvRequestListenersCreate(int dbnum, requestListeners *parent);

/**
 * @brief 从字典中查找指定 key 对应的值（便捷接口）
 * @param d   dict_t 指针
 * @param key 查找键指针
 * @return 找到返回值指针；未找到返回 NULL
 */
void *dictFetchValue(dict_t *d, const void *key);

/**
 * @brief 将监听器节点链接到父节点（递增父链 nlisteners 计数）
 * @param listeners 要链接的监听器节点指针
 */
void requestListenersLink(requestListeners *listeners);

/**
 * @brief 单个请求监听器结构体（当前为占位结构，待扩展）
 */
typedef struct requestListener {
    /* data */
} requestListener;

/**
 * @brief 将单个监听器追加到监听器节点的回调链表中
 * @param listeners 目标监听器节点指针
 * @param listener  要追加的 requestListener 指针
 */
void requestListenersPush(requestListeners *listeners,
        requestListener *listener);
