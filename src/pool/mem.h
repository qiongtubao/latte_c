#ifndef LATTE_C_POOL_H
#define LATTE_C_POOL_H

#include "mutex/mutex.h"
#include "sds/sds.h"
#include "set/set.h"
#include "list/list.h"

/**
 * @brief 内存池结构体
 *
 * 采用预分配 + 空闲列表策略，支持线程安全和动态扩容：
 * - 每次扩容分配一块连续大内存（item_size * item_num_per_pool 字节），
 *   拆分为 item_num_per_pool 个小对象加入 frees 空闲列表
 * - 所有已分配大内存块记录在 pools 链表中，释放时统一清理
 * - 已使用对象记录在 used 集合中，防止重复释放
 */
typedef struct mem_pool_t {
    latte_mutex_t* mutex;     /**< 递归互斥锁，保证线程安全 */
    sds name;                 /**< 内存池名称，用于日志标识 */
    bool dynamic;             /**< 是否允许动态扩容：true 时空闲耗尽自动扩容 */
    int size;                 /**< 当前总对象数量（所有 pool 块的对象总和） */
    int item_size;            /**< 每个对象的字节大小 */
    int item_num_per_pool;    /**< 每次扩容时分配的对象数量 */
    list_t* pools;            /**< 所有分配的大内存块链表，释放时统一 zfree */
    set_t* used;              /**< 已分配（在用）对象的集合，用于合法性校验 */
    list_t* frees;            /**< 空闲对象链表，alloc 时从尾部取，free 时追加尾部 */
} mem_pool_t;

/**
 * @brief 创建一个新的内存池（未初始化，需调用 mem_pool_init）
 * @param name 内存池名称（SDS 字符串，由池管理，无需调用方释放）
 * @return mem_pool_t* 成功返回内存池指针，失败返回 NULL
 */
mem_pool_t* mem_pool_new(sds name);

/**
 * @brief 销毁内存池并释放所有内存
 * 仅当所有已分配对象都已归还（frees 长度 == size）时才能成功删除。
 * @param pool 目标内存池指针
 * @return int 成功返回 1，仍有未归还对象时返回 0
 */
int mem_pool_delete(mem_pool_t* pool);

/**
 * @brief 清理内存池所有内部资源（used/frees/pools 链表及其内存块）
 * 将 size 重置为 0，不释放 pool 结构体本身及 mutex。
 * @param mem_pool 目标内存池指针
 */
void mem_pool_clean_up(mem_pool_t* mem_pool);

/**
 * @brief 扩容：分配一块大内存并拆分加入空闲列表
 * 仅 dynamic=true 时允许调用，否则返回 -1 并记录错误日志。
 * @param pool 目标内存池指针
 * @return int 成功返回 0，失败返回 -1
 */
int mem_pool_extend(mem_pool_t* pool);

/**
 * @brief 从内存池分配一个对象
 * 空闲列表为空且 dynamic=true 时自动扩容。
 * @param mem_pool 目标内存池指针
 * @return void* 成功返回对象指针，失败（无空闲且不可扩容）返回 NULL
 */
void* mem_pool_alloc(mem_pool_t* mem_pool);

/**
 * @brief 将已分配的对象归还到内存池空闲列表
 * 若 result 不在 used 集合中（重复归还或非法指针），记录警告日志并返回。
 * @param mem_pool 目标内存池指针
 * @param result   要归还的对象指针
 */
void mem_pool_free(mem_pool_t* mem_pool, void* result);

/**
 * @brief 初始化内存池参数并预分配内存
 * @param mem_pool         目标内存池指针（已由 mem_pool_new 创建）
 * @param item_size        每个对象的字节大小（必须 > 0）
 * @param dynamic          是否允许动态扩容
 * @param pool_num         初始预分配的 pool 块数量（必须 > 0）
 * @param item_num_per_pool 每个 pool 块包含的对象数量（必须 > 0）
 * @return int 成功返回 0，参数非法或内存分配失败返回 -1
 */
int mem_pool_init(mem_pool_t* mem_pool, int item_size, bool dynamic, int pool_num, int item_num_per_pool);
#endif