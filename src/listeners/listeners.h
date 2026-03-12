
#include "../sds/sds.h"
#include "../dict/dict.h"
#include "../list/list.h"

/** @brief 简化数据库标识类型，用整型表示数据库编号 */
typedef int redisDb;
/** @brief 简化键对象类型，用字符类型表示 Redis 对象 */
typedef char robj;

/** @brief 请求监听器挂载层级：服务器级别 */
#define REQUEST_LEVEL_SVR 0
/** @brief 请求监听器挂载层级：数据库级别 */
#define REQUEST_LEVEL_DB 1
/** @brief 请求监听器挂载层级：键级别 */
#define REQUEST_LEVEL_KEY 2

/**
 * @brief 请求监听器树节点结构体
 *
 * 以树形结构组织监听器，支持 服务器/数据库/键 三个层级。
 * 每个节点维护当前层级以及子层级的监听器总数，方便快速判断。
 */
typedef struct requestListeners {
  list_t *listeners;                  /**< 当前节点绑定的监听器链表 */
  struct requestListeners *parent;    /**< 父节点指针，用于向上递减计数 */
  int nlisteners;                     /**< 当前节点及所有子节点的监听器总数 */
  int level;                          /**< 当前节点层级：key/db/svr */
  union {
      /** 服务器层级数据：存储数据库数量及各数据库监听节点数组 */
      struct {
          int dbnum;                       /**< 数据库总数 */
          struct requestListeners **dbs;   /**< 指向各数据库监听节点的指针数组 */
      } svr;
      /** 数据库层级数据：存储数据库编号及键->监听节点的字典 */
      struct {
          redisDb db;      /**< 数据库编号 */
          dict_t *keys;    /**< 键到键级监听节点的映射字典 */
      } db;
      /** 键层级数据：存储该监听节点对应的键对象 */
      struct {
          robj *key;       /**< 当前监听的键对象指针 */
      } key;
  };
} requestListeners;

/**
 * @brief 根据 db 和 key 在监听树中查找或创建对应的监听节点
 * @param root   根节点（服务器级别）
 * @param db     数据库编号，负数表示仅返回服务器级别节点
 * @param key    键对象，NULL 表示仅返回数据库级别节点
 * @param create 当键级监听节点不存在时是否创建
 * @return requestListeners* 返回找到或新建的监听节点，失败返回 NULL
 */
requestListeners* requestBindListeners(requestListeners *root, redisDb db, robj *key, int create);

/**
 * @brief 创建服务器级别的监听器树根节点
 * @param dbnum  数据库总数，将为每个 db 创建对应的 db 级节点
 * @param parent 父节点指针，通常为 NULL
 * @return requestListeners* 返回新创建的服务器级监听节点
 */
requestListeners *srvRequestListenersCreate(int dbnum, requestListeners *parent);

/**
 * @brief 从字典中获取键对应的值
 * @param d   目标字典指针
 * @param key 要查找的键
 * @return void* 找到时返回对应值指针，未找到返回 NULL
 */
void *dictFetchValue(dict_t *d, const void *key);

/**
 * @brief 向上递增监听计数（从当前节点到根节点）
 * @param listeners 起始节点指针
 */
void requestListenersLink(requestListeners *listeners);

/**
 * @brief 单个请求监听器结构体（占位，具体字段待扩展）
 */
typedef struct requestListener
{
    /* data */
} requestListener;

/**
 * @brief 向监听节点推入一个新的监听器并更新计数
 * @param listeners 目标监听节点
 * @param listener  要推入的监听器指针
 */
void requestListenersPush(requestListeners *listeners,
        requestListener *listener);