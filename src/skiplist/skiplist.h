
#include "../utils/atomic.h"
#include "../sds/sds.h"

/**
 * @brief 跳跃表节点比较函数类型
 * @param a_ele 节点 a 的键字符串
 * @param a     节点 a 的值指针
 * @param b_ele 节点 b 的键字符串
 * @param b     节点 b 的值指针
 * @return int 负数表示 a < b，0 表示相等，正数表示 a > b
 */
typedef int (comparator)(sds_t a_ele, void* a, sds_t b_ele, void* b);

/**
 * @brief 释放节点值的回调函数类型
 * @param value 节点中存储的值指针
 */
typedef void (freeValue)(void* value);

/**
 * @brief 跳跃表节点结构体
 *
 * 每个节点包含：
 * - ele：该节点对应的键（SDS 字符串）
 * - score：用户关联的值指针（作为排序依据）
 * - backward：指向前一个节点的指针（仅第 0 层使用，支持逆向遍历）
 * - level[]：柔性数组，每层存储前向指针和跨度
 */
typedef struct skiplist_node_t {
    sds_t ele;                          /**< 节点键，用于比较和查找 */
    void* score;                        /**< 节点关联值（由用户提供的比较器排序） */
    struct skiplist_node_t *backward;   /**< 第 0 层反向指针，用于逆向遍历 */
    struct skiplistLevel {
        struct skiplist_node_t *forward; /**< 该层的前向指针 */
        unsigned long span;              /**< 该层跳跃的节点数（用于 rank 计算） */
    } level[];                          /**< 每层的指针和跨度信息 */
} skiplist_node_t;

/**
 * @brief 跳跃表结构体
 *
 * Redis 风格的跳跃表实现，支持 O(log N) 插入、删除、查找。
 * header 为哨兵节点（不存储实际数据），tail 指向最后一个真实节点。
 */
typedef struct skiplist_t {
    struct skiplist_node_t *header; /**< 哨兵头节点，不存储实际数据 */
    struct skiplist_node_t *tail;   /**< 指向最后一个节点 */
    unsigned long length;           /**< 跳跃表中实际节点数（不含 header） */
    int level;                      /**< 当前最高层数 */
    comparator* comparator;         /**< 节点比较函数指针 */
} skiplist_t;

/**
 * @brief 跳跃表迭代器
 * @note 目前仅用于持有当前遍历位置，函数接口未完全实现
 */
typedef struct skiplistIter {
    struct skiplist_t* list;        /**< 所属跳跃表 */
    struct skiplist_node_t* node;   /**< 当前节点指针 */
} skiplistIter;

/**
 * @brief 范围查询规格，用于 zrangeByScore 等区间查询
 */
typedef struct {
    void *min, *max;    /**< 区间最小值和最大值 */
    int minex, maxex;   /**< 是否为开区间：1=开区间（不含端点），0=闭区间 */
} zrangespec;

/**
 * @brief 创建一个新的跳跃表
 * @param comparator 节点比较函数
 * @return skiplist_t* 新建跳跃表指针
 */
struct skiplist_t* skipListNew(comparator comparator);

/**
 * @brief 释放整个跳跃表及其所有节点
 * @param sl      目标跳跃表
 * @param free_fn 节点值释放回调（可为 NULL）
 */
void skipListFree(skiplist_t* sl, freeValue* free_fn);

/**
 * @brief 创建一个新的跳跃表节点
 * @param level 该节点的层数
 * @param value 节点关联值
 * @param ele   节点键（SDS 字符串）
 * @return skiplist_node_t* 新节点指针
 */
skiplist_node_t* skipListCreateNode(int level, void* value, sds_t ele);

/**
 * @brief 释放一个跳跃表节点并返回其值
 * @param node 要释放的节点
 * @return void* 节点存储的值指针
 */
void* skipListFreeNode(skiplist_node_t* node);

/**
 * @brief 查找区间内的第一个节点（最小值方向）
 * @param sl 目标跳跃表
 * @param zs 范围规格
 * @return skiplist_node_t* 满足条件的第一个节点，未找到返回 NULL
 */
skiplist_node_t* skiplist_firstInRange(skiplist_t* sl, zrangespec* zs);

/**
 * @brief 查找区间内的最后一个节点（最大值方向）
 * @param sl 目标跳跃表
 * @param zs 范围规格
 * @return skiplist_node_t* 满足条件的最后一个节点，未找到返回 NULL
 */
skiplist_node_t* skiplist_lastInRange(skiplist_t* sl, zrangespec* zs);

/**
 * @brief 向跳跃表插入一个节点
 * @param skiplist 目标跳跃表
 * @param value    节点关联值
 * @param ele      节点键（SDS 字符串）
 * @return skiplist_node_t* 插入的新节点指针
 */
skiplist_node_t* skipListInsert(skiplist_t* skiplist, void* value, sds_t ele);

/**
 * @brief 从跳跃表删除指定节点
 * @param skiplist 目标跳跃表
 * @param val      节点值
 * @param ele      节点键
 * @param node     输出被删除的节点指针（可为 NULL）
 * @return void*   被删除节点的值，未找到返回 NULL
 */
void* skipListDelete(skiplist_t* skiplist, void* val, sds_t ele, skiplist_node_t** node);

/**
 * @brief 判断 score 是否 >= 范围最小值（考虑开/闭区间）
 * @param sl    目标跳跃表
 * @param score 待比较值
 * @param zs    范围规格
 * @return int  满足条件返回 1，否则返回 0
 */
int slValueGteMin(skiplist_t* sl, void* score, zrangespec* zs);

/**
 * @brief 判断 score 是否 <= 范围最大值（考虑开/闭区间）
 * @param sl    目标跳跃表
 * @param score 待比较值
 * @param zs    范围规格
 * @return int  满足条件返回 1，否则返回 0
 */
int slValueLteMax(skiplist_t* sl, void* score, zrangespec* zs);
