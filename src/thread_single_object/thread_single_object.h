

#ifndef __LATTE_THREAD_SINGLE_OBJECT_H
#define __LATTE_THREAD_SINGLE_OBJECT_H

#include "dict/dict.h"
#include "sds/sds.h"
#include <pthread.h>

/**
 * @brief 线程局部单例对象管理器
 *
 * 基于 pthread_key（线程本地存储）实现：
 * - 注册表（dict）保存名称 → 销毁函数的映射（全局共享）
 * - 每个线程拥有独立的对象字典（通过 pthread_key 隔离）
 * - 对象和销毁函数不是 1:1 关系，多个对象可共用同一销毁函数
 */
typedef struct thread_single_object_manager_t {
    dict_t* dict;                       /**< 全局注册表：name → thread_single_entry_t（含销毁函数） */
    pthread_key_t latte_thread_key;     /**< 线程本地存储键，每线程存储各自的对象字典 dict_t* */
} thread_single_object_manager_t;

/** @brief 全局单例管理器指针 */
extern thread_single_object_manager_t* latte_thread_single_object_manager;

/**
 * @brief 对象注册条目：存储对象的销毁回调
 */
typedef struct thread_single_entry_t {
    void (*destroy)(void* arg);  /**< 对象销毁回调函数 */
} thread_single_entry_t;

/**
 * @brief 初始化全局线程局部单例管理器
 * 创建注册表字典和 pthread_key，已初始化时直接返回。
 */
void thread_single_object_module_init();

/**
 * @brief 销毁全局管理器及当前线程的所有对象
 * 遍历当前线程对象字典，依次调用各对象的销毁函数，
 * 然后删除 pthread_key 和注册表。
 */
void thread_single_object_module_destroy();

/**
 * @brief 注册一个线程局部对象类型及其销毁函数
 * @param name    对象名称（字符串键）
 * @param destroy 对象销毁回调函数
 */
void thread_single_object_register(char* name, void (*destroy)(void* arg));

/**
 * @brief 注销一个线程局部对象类型
 * 从注册表移除条目；若注册表为空，则自动销毁管理器。
 * @param name 对象名称
 */
void thread_single_object_unregister(char* name);

/**
 * @brief 获取当前线程中指定名称的对象值
 * @param name 对象名称
 * @return void* 对象指针，未设置或当前线程无对象返回 NULL
 */
void* thread_single_object_get(char* name);

/**
 * @brief 在当前线程中设置指定名称的对象值
 * 若当前线程尚无对象字典，则按需创建并绑定到 pthread_key。
 * @param name  对象名称（必须已注册）
 * @param value 对象指针
 * @return void* 返回传入的 value
 */
void* thread_single_object_set(char* name, void* value);

/**
 * @brief 删除当前线程中指定名称的对象并调用其销毁函数
 * 若对象字典为空则释放字典并清除 pthread_key。
 * @param name 对象名称
 * @return int 成功删除返回 1，对象不存在返回 0
 */
int thread_single_object_delete(char* name);
#endif
