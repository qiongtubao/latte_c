/**
 * @file thread_single_object.c
 * @brief 线程局部单例对象管理实现文件
 * @details 实现了线程本地存储的单例对象管理，支持对象注册、获取和自动销毁
 * @author latte_c
 * @date 2026-03-12
 */

#include "thread_single_object.h"
#include "dict/dict_plugins.h"
#include "debug/latte_debug.h"
#include "utils/utils.h"

/**
 * @brief 创建一个注册条目（内部辅助函数）
 * @param destroy 对象销毁回调函数
 * @return thread_single_entry_t* 新建条目指针
 */
thread_single_entry_t* thread_single_entry_new(void (*destroy)(void* arg)) {
    thread_single_entry_t* entry = (thread_single_entry_t*)zmalloc(sizeof(thread_single_entry_t));
    entry->destroy = destroy;
    return entry;
}

/**
 * @brief 释放注册条目（内部辅助函数）
 * @param entry 目标条目指针
 */
void thread_single_entry_delete(thread_single_entry_t* entry) {
    zfree(entry);
}

/**
 * @brief 字典析构回调：释放注册表中的 thread_single_entry_t 值
 * 由 dict_func_t.valDestructor 调用。
 * @param dict  字典指针（未使用）
 * @param value thread_single_entry_t 指针
 */
void dict_thread_single_object_register_destructor(dict_t* dict, void* value) {
    thread_single_entry_delete((thread_single_entry_t*)value);
}

/** @brief 全局注册表的字典操作函数（char* 键，entry 值带析构） */
dict_func_t dict_thread_single_register_object = {
    .hashFunction = dict_char_hash,
    .keyCompare = dict_char_key_compare,
    .valDestructor = dict_thread_single_object_register_destructor,
};

/** @brief 全局线程局部单例管理器实例（初始为 NULL，由 module_init 创建） */
thread_single_object_manager_t* thread_single_object_manager = NULL;

/**
 * @brief 初始化全局线程局部单例管理器
 * 若已初始化则直接返回（幂等）。
 * 创建注册表字典和 pthread_key（线程退出时不自动析构）。
 */
void thread_single_object_module_init() {
    if (thread_single_object_manager != NULL) {
        return;
    }
    thread_single_object_manager = (thread_single_object_manager_t*)zmalloc(sizeof(thread_single_object_manager_t));
    thread_single_object_manager->dict = dict_new(&dict_thread_single_register_object);
    pthread_key_create(&thread_single_object_manager->latte_thread_key, NULL);
}

/**
 * @brief 调用指定对象的销毁函数（内部辅助函数）
 * 从注册表查找对象类型的析构函数，调用后由调用方负责从对象字典移除。
 * @param name  对象名称
 * @param value 对象指针
 */
void thread_single_object_destroy(char* name, void* value) {
    dict_entry_t* entry = dict_find(thread_single_object_manager->dict, name);
    latte_assert(entry != NULL) ;
    thread_single_entry_t* entry_obj = (thread_single_entry_t*)dict_get_entry_val(entry);
    latte_assert(entry_obj != NULL);
    latte_assert(entry_obj->destroy != NULL);
    entry_obj->destroy(value);
}

/**
 * @brief 销毁全局管理器及当前线程的所有对象
 *
 * 1. 获取当前线程的对象字典（pthread_getspecific）
 * 2. 遍历字典，对每个对象调用对应销毁函数
 * 3. 删除对象字典并清空 pthread_key
 * 4. 删除 pthread_key 和全局注册表，释放管理器
 */
void thread_single_object_module_destroy() {
    if (thread_single_object_manager == NULL) {
        return;
    }
    /* 销毁当前线程的所有对象 */
    dict_t* object = (dict_t*)pthread_getspecific(thread_single_object_manager->latte_thread_key);
    if (object != NULL) {
        latte_iterator_t* iter = dict_get_latte_iterator(object);
        while (latte_iterator_has_next(iter)) {
            latte_pair_t* val = latte_iterator_next(iter);
            if (val != NULL) {
                thread_single_object_destroy(val->key, val->value);
            }
        }
        dict_delete(object);
        pthread_setspecific(thread_single_object_manager->latte_thread_key, NULL);
    }

    /* 释放全局管理器资源 */
    if (thread_single_object_manager != NULL) {
        pthread_key_delete(thread_single_object_manager->latte_thread_key);
        dict_delete(thread_single_object_manager->dict);
        zfree(thread_single_object_manager);
        thread_single_object_manager = NULL;
    }
}

/**
 * @brief 注册一个线程局部对象类型及其销毁函数
 * 将 name → thread_single_entry_t 插入全局注册表。
 * @param name    对象名称（字符串键，不拷贝，调用方保证生命周期）
 * @param destroy 对象销毁回调函数
 */
void thread_single_object_register(char* name, void (*destroy)(void* arg)) {
    latte_assert(thread_single_object_manager != NULL);
    thread_single_entry_t* entry = thread_single_entry_new(destroy);
    entry->destroy = destroy;
    latte_assert_with_info(DICT_OK == dict_add(thread_single_object_manager->dict, name, entry), "thread_single_object_register failed");
}

/**
 * @brief 注销一个线程局部对象类型
 * 从全局注册表删除条目；若注册表为空，自动销毁管理器（隐式 module_destroy）。
 * @param name 对象名称
 */
void thread_single_object_unregister(char* name) {
    latte_assert(thread_single_object_manager != NULL);
    int result = dict_delete_key(thread_single_object_manager->dict, name);
    latte_assert_with_info(DICT_OK == result , "thread_single_object_unregister failed");
    if (dict_size(thread_single_object_manager->dict) == 0) {
        thread_single_object_module_destroy();
    }
}

/** @brief 线程对象字典的操作函数（仅 hashFunction 和 keyCompare，值无析构） */
dict_func_t dict_thread_single_object_func = {
    .hashFunction = dict_char_hash,
    .keyCompare = dict_char_key_compare
};

/**
 * @brief 获取当前线程中指定名称的对象值
 * 通过 pthread_getspecific 获取当前线程的对象字典，再查找 name 键。
 * @param name 对象名称
 * @return void* 对象指针，未找到返回 NULL
 */
void* thread_single_object_get(char* name) {
    latte_assert_with_info(thread_single_object_manager != NULL, "thread_single_object_manager is not initialized");
    dict_t* object = (dict_t*)pthread_getspecific(thread_single_object_manager->latte_thread_key);
    if (object == NULL) {
        return NULL;
    }
    dict_entry_t* entry = dict_find(object, name);
    if (entry == NULL) {
        return NULL;
    }
    return dict_get_entry_val(entry);
}

/**
 * @brief 删除当前线程中指定名称的对象并调用销毁函数
 * 若删除后对象字典为空，则释放字典并清除 pthread_key。
 * @param name 对象名称
 * @return int 成功返回 1，对象字典不存在或对象不存在返回 0
 */
int thread_single_object_delete(char* name) {
    latte_assert_with_info(thread_single_object_manager != NULL, "thread_single_object_manager is not initialized");
    dict_t* object = (dict_t*)pthread_getspecific(thread_single_object_manager->latte_thread_key);
    if (object == NULL) {
        return 0;
    }
    dict_entry_t* entry = dict_find(object, name);
    void* value = dict_get_entry_val(entry);
    if (value == NULL) {
        return 0;
    }
    thread_single_object_destroy(name, value); /* 调用注册的销毁函数 */
    dict_delete_key(object, name);
    /* 若字典已空，释放字典并解除线程本地绑定 */
    if (dict_size(object) == 0) {
        dict_delete(object);
        pthread_setspecific(thread_single_object_manager->latte_thread_key, NULL);
    }
    return 1;
}

/**
 * @brief 在当前线程中设置指定名称的对象值
 *
 * 检查 name 是否已注册，若当前线程尚无对象字典则按需创建并绑定。
 * 最后将 name → value 插入线程对象字典。
 * @param name  对象名称（必须已通过 register 注册）
 * @param value 对象指针
 * @return void* 返回传入的 value
 */
void* thread_single_object_set(char* name, void* value) {
    latte_assert_with_info(thread_single_object_manager != NULL, "thread_single_object_manager is not initialized");
    dict_entry_t* entry = dict_find(thread_single_object_manager->dict, name);
    latte_assert_with_info(entry != NULL, "thread_single_object_set failed");
    /* 获取或创建当前线程的对象字典 */
    dict_t* object = (dict_t*)pthread_getspecific(thread_single_object_manager->latte_thread_key);
    if (object == NULL) {
        object = dict_new(&dict_thread_single_object_func);
        pthread_setspecific(thread_single_object_manager->latte_thread_key, object);
    }
    dict_add(object, name, value);
    return value;
}