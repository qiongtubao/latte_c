


# 功能

# 数据结构
## sdshdr5
| len(5) | type(3) |  value |
          

## sdshdr8,16,32,64
|   used_len  |   alloc  | type(8) |  value |

# 性能
| 字符串大小 ｜ 系统 ｜内存分配器 ｜ 平均耗时(1亿) ｜
｜ 7 ｜ macos ｜ libc ｜ ｜
# API
* sdsnew （申请sds字符串）
* sdsfree （释放sds）
