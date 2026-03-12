#ifndef __LATTE_OBJECT_H__
#define __LATTE_OBJECT_H__

/** @brief LRU 时间戳字段位数（24 位，相对于全局 lru_clock） */
#define LRU_BITS 24
/** @brief 引用计数字段位数（32 位） */
#define OBJ_REFCOUNT_BITS 32

struct object_manager_t;

/**
 * @brief latte 通用对象结构体
 *
 * 采用位域紧凑存储，兼容 Redis 的 robj 布局：
 * - type:     对象类型 ID（8 位，由 object_manager 在创建时写入）
 * - lru:      LRU 访问时间戳或 LFU 数据（24 位）
 * - refcount: 引用计数（32 位），归 0 时联动 object_manager 释放
 * - ptr:      指向实际数据的指针
 */
typedef struct latte_object_t {
    unsigned type:8;            /**< 对象类型 ID，由 object_manager 写入 */
    unsigned lru:LRU_BITS;      /**< LRU 时间（相对于全局 lru_clock）或
                                 *   LFU 数据（低 8 位为访问频率，高 16 位为访问时间） */
    unsigned refcount : OBJ_REFCOUNT_BITS; /**< 引用计数，归 0 时自动释放 */
    void *ptr;                  /**< 指向实际对象数据的指针 */
} latte_object_t;

/**
 * @brief 增加对象引用计数（原子性 +1）
 * @param o 目标对象指针，NULL 时安全返回
 */
void latte_object_incr_ref_count(latte_object_t *o);

/**
 * @brief 减少对象引用计数，引用计数归 0 时联动 object_manager 释放对象
 * @param o 目标对象指针，NULL 或 refcount 已为 0 时安全返回
 */
void latte_object_decr_ref_count(latte_object_t *o);
#endif