/**
 * @file dir.c
 * @brief 目录操作实现
 *        提供目录创建、递归创建、检查及文件扫描等功能
 */
#include <stdio.h>
#include <sys/stat.h> /* 包含 mkdir 函数声明 */
#include <errno.h>    /* 包含 errno 和错误码 */
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include "dir.h"
#include "list/list.h"
#include "iterator/iterator.h"
#include <regex.h>
#include "log/log.h"

/**
 * @brief 创建目录
 * @param path 目录路径
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* dirCreate(char* path) {
    /* 创建目录 */
    int status = mkdir(path, 0755);
    /* 检查 mkdir 函数的返回值 */
    if (status != 0) {
        if (errno == EEXIST) {
            /* 已经存在了，不报错 */
            return &Ok;
        }
        return errno_io_new("create dir fail");
    }
    return &Ok;
}

/**
 * @brief 字符串复制函数（安全版本）
 * @param s 要复制的字符串
 * @return 复制的字符串指针，失败返回 NULL
 */
char* my_strdup(const char *s) {
    if (s == NULL) {
        return NULL;
   }

    size_t length = strlen(s) + 1; /* 包括终止符 '\0' */
    char *copy = (char *)zmalloc(length);

    if (copy != NULL) {
        memcpy(copy, s, length);
    }

    copy[strlen(s)] = '\0';
    return copy;
}

/**
 * @brief 递归创建目录
 *        逐层创建路径中的所有目录
 * @param path 目录路径
 * @param mode 目录权限
 * @return 错误对象指针，成功时返回 &Ok
 */
Error* dirCreateRecursive(const char* path, mode_t mode) {
    char *buf = NULL;
    size_t len;
    Error* status = &Ok;
    size_t i;
    buf = my_strdup(path);
    if (buf == NULL) {
        return errno_io_new("strdup");
    }

    len = strlen(buf) + 1;
    for (i = 1; i < len; ++i) {
        if (buf[i] == '/' || buf[i] == '\\') {
            buf[i] = '\0';
            if (mkdir(buf, mode) == -1 && errno != EEXIST) {
                status =  errno_io_new("mkdir");
                break;
            }
            buf[i] = '/';
        }
    }
    if (mkdir(buf, mode) == -1 && errno != EEXIST) {
        status =  errno_io_new("mkdir");
    }
    zfree(buf);
    return status;
}

/**
 * @brief 检查路径是否为目录
 * @param path 要检查的路径
 * @return 1 表示是目录，0 表示不是目录或路径不存在
 */
int dirIs(char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        /* 如果路径不存在或无法访问，返回错误 */
        return 0;
    }
    /* S_ISDIR 宏检查是否为目录 */
    return S_ISDIR(st.st_mode);
}

#define LATTE_PATH_MAX 4096

/**
 * @brief 扫描目录中的文件
 *        根据正则表达式过滤器扫描指定目录中的文件（不包括子目录和隐藏文件）
 * @param dir_path       目录路径
 * @param filter_pattern 正则表达式过滤器（可为 NULL）
 * @return 包含文件名的迭代器，失败返回 NULL
 */
latte_iterator_t* dir_scan_file(char* dir_path, const char* filter_pattern) {
  regex_t reg;
  if (filter_pattern) {
    const int res = regcomp(&reg, filter_pattern, REG_NOSUB);
    if (res) {
      char errbuf[256];
      regerror(res, &reg, errbuf, sizeof(errbuf));
      LATTE_LIB_LOG(LOG_ERROR,"regcomp return error. filter pattern %s. errmsg %d:%s", filter_pattern, res, errbuf);
      return NULL;
    }
  }
  DIR *pdir = opendir(dir_path);
  if (!pdir) {
    if (filter_pattern) {
      regfree(&reg);
      LATTE_LIB_LOG(LOG_ERROR,"open directory failure. path %s, errmsg %d:%s", dir_path, errno, strerror(errno));
      return NULL;
    }
  }
  struct dirent *pentry;
  list_t* files = list_new();
  char tmp_path[LATTE_PATH_MAX];
  while ((pentry = readdir(pdir)) != NULL) {
    if ('.' == pentry->d_name[0]) /* 跳过 ./ .. 文件和隐藏文件 */
      continue;
    snprintf(tmp_path, sizeof(tmp_path), "%s/%s", dir_path, pentry->d_name);
    if (dirIs(tmp_path))
      continue;

    if (!filter_pattern || 0 == regexec(&reg, pentry->d_name, 0, NULL, 0))
      list_add_node_tail(files, sds_new(pentry->d_name));
  }
  if (filter_pattern)
    regfree(&reg);
  closedir(pdir);
  return list_get_latte_iterator(files, LIST_ITERATOR_OPTION_HEAD | LIST_ITERATOR_OPTION_DELETE_LIST);
}
