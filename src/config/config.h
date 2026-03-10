/*
 * config.h - 配置管理头文件
 * 
 * Latte C 库核心组件：配置文件解析和管理
 * 
 * 核心功能：
 * 1. 键值对配置存储
 * 2. 支持多种类型 (string/int/bool/float)
 * 3. 配置文件加载 (INI 风格或 KEY=VALUE)
 * 4. 环境变量覆盖
 * 5. 配置项默认值
 * 6. 配置变更回调
 * 
 * 使用流程：
 * 1. config_new() - 创建配置对象
 * 2. config_load() - 从文件加载
 * 3. config_get_*() - 获取配置值
 * 4. config_set_*() - 设置配置值
 * 5. config_save() - 保存回文件
 * 6. config_delete() - 释放资源
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef __LATTE_CONFIG_H
#define __LATTE_CONFIG_H

#include <stddef.h>

/* 配置对象前向声明 */
typedef struct config_t config_t;

/* 配置变更回调函数类型 */
typedef void (*config_change_cb)(const char *key, const char *old_value, const char *new_value, void *user_data);

/* 创建新配置对象
 * 返回：成功返回配置指针，失败返回 NULL
 */
config_t* config_new(void);

/* 从文件加载配置
 * 参数：config - 配置对象
 *       path - 配置文件路径
 * 返回：成功返回 0，失败返回 -1
 * 支持的格式：KEY=VALUE 每行一个，#开头为注释
 */
int config_load(config_t *config, const char *path);

/* 从字符串加载配置
 * 参数：config - 配置对象
 *       content - 配置内容字符串
 * 返回：成功返回 0，失败返回 -1
 */
int config_load_string(config_t *config, const char *content);

/* 获取字符串配置值
 * 参数：config - 配置对象
 *       key - 键名
 *       default_val - 默认值 (未找到时返回)
 * 返回：配置值或默认值
 */
const char* config_get_string(config_t *config, const char *key, const char *default_val);

/* 获取整数配置值
 * 参数：config - 配置对象
 *       key - 键名
 *       default_val - 默认值
 * 返回：配置值或默认值
 */
int64_t config_get_int(config_t *config, const char *key, int64_t default_val);

/* 获取布尔配置值
 * 参数：config - 配置对象
 *       key - 键名
 *       default_val - 默认值 (true/false)
 * 返回：配置值或默认值
 * 识别值：true/yes/1/on = true, false/no/0/off = false
 */
int config_get_bool(config_t *config, const char *key, int default_val);

/* 获取浮点配置值
 * 参数：config - 配置对象
 *       key - 键名
 *       default_val - 默认值
 * 返回：配置值或默认值
 */
double config_get_double(config_t *config, const char *key, double default_val);

/* 设置字符串配置值
 * 参数：config - 配置对象
 *       key - 键名
 *       value - 值
 * 返回：成功返回 0，失败返回 -1
 */
int config_set_string(config_t *config, const char *key, const char *value);

/* 设置整数配置值
 * 参数：config - 配置对象
 *       key - 键名
 *       value - 值
 * 返回：成功返回 0，失败返回 -1
 */
int config_set_int(config_t *config, const char *key, int64_t value);

/* 设置布尔配置值
 * 参数：config - 配置对象
 *       key - 键名
 *       value - 值 (true/false)
 * 返回：成功返回 0，失败返回 -1
 */
int config_set_bool(config_t *config, const char *key, int value);

/* 删除配置项
 * 参数：config - 配置对象
 *       key - 键名
 */
void config_remove(config_t *config, const char *key);

/* 保存配置到文件
 * 参数：config - 配置对象
 *       path - 目标文件路径
 * 返回：成功返回 0，失败返回 -1
 */
int config_save(config_t *config, const char *path);

/* 设置配置变更回调
 * 参数：config - 配置对象
 *       cb - 回调函数
 *       user_data - 用户数据 (传递给回调)
 */
void config_set_change_callback(config_t *config, config_change_cb cb, void *user_data);

/* 检查配置项是否存在
 * 参数：config - 配置对象
 *       key - 键名
 * 返回：存在返回 1，不存在返回 0
 */
int config_has(config_t *config, const char *key);

/* 获取所有键名列表
 * 参数：config - 配置对象
 * 返回：键名数组，调用者负责释放
 */
char** config_get_keys(config_t *config, size_t *count);

/* 删除配置对象
 * 参数：config - 待删除的配置对象
 */
void config_delete(config_t *config);

/* 获取配置项数量
 * 参数：config - 配置对象
 * 返回：配置项数量
 */
size_t config_count(config_t *config);

#endif /* __LATTE_CONFIG_H */
