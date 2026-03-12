/**
 * @file ziplist.h
 * @brief 压缩列表（ziplist）接口
 *        ziplist 是一种连续内存的紧凑列表格式，将字符串和整数元素顺序存储在单块内存中，
 *        支持从头尾两端高效插入/删除。适用于元素较少时节省内存（如 hash、list 的紧凑实现）。
 *        内存布局：[zlbytes(4B)][zltail(4B)][zllen(2B)][entry...][zlend(1B, 0xFF)]
 */

#ifndef __LATTE_ZIP_LIST_H
#define __LATTE_ZIP_LIST_H

/**
 * @brief ziplist 条目结构体
 *        用于在遍历时表示单个元素，根据编码类型，实际存储为字符串（sval）或整数（lval）。
 */
typedef struct {
    unsigned char *sval; /**< 字符串值指针（整数类型时为 NULL） */
    unsigned int slen;   /**< 字符串长度（整数类型时为 0） */
    long long lval;      /**< 整数值（字符串类型时未定义） */
} zip_list_entry_t;

/**
 * @brief 创建新的空 ziplist
 * @return 新建的 ziplist 指针（unsigned char*）；内存分配失败返回 NULL
 */
unsigned char *zip_list_new(void);

/**
 * @brief 删除 ziplist 中指定位置的元素
 *        不释放 *zl 内存，仅删除 *p 指向的数据条目并更新 ziplist 头信息。
 * @param zl  ziplist 指针（可能因 realloc 移动）
 * @param p   指向要删除元素的指针（删除后更新为下一个元素位置）
 * @return 更新后的 ziplist 指针（可能与 zl 不同）
 */
unsigned char *zip_list_delete(unsigned char *zl, unsigned char **p);


#endif
