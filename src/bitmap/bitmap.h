/*
 * bitmap.h - bitmap 模块头文件
 * 
 * Latte C 库组件
 * 
 * 作者：自动注释生成
 * 日期：2026-03-08
 */

#ifndef __LATTE_BITMAP_H
#define __LATTE_BITMAP_H

#include "sds/sds.h"
#include <stdbool.h>
#include "iterator/iterator.h"

#define bitmap_t sds
#define bit_size sds_len

bitmap_t bitmap_new(int size);
void bitmap_clear_all(bitmap_t map);
bool bitmap_get(bitmap_t map, int index);
void bitmap_set(bitmap_t map, int index);
void bitmap_clear(bitmap_t map, int index);
int bitmap_next_unsetted(bitmap_t map, int start);
int bitmap_next_setted(bitmap_t map, int start);
latte_iterator_t* bitmap_get_setted_iterator(bitmap_t map, int start);
latte_iterator_t* bitmap_get_unsetted_iterator(bitmap_t map, int start);

#endif
