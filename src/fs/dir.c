#include <stdio.h>
#include <sys/stat.h> // 包含mkdir函数声明
#include <errno.h>    // 包含errno和错误码
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

Error* dirCreate(char* path) {
    // 创建目录
    int status = mkdir(path, 0755);
    // 检查mkdir函数的返回值
    if (status != 0) {
        if (errno == EEXIST) {
            //已经存在了，不报错
            return &Ok;
        } 
        return errnoIoCreate("create dir fail");
    }
    return &Ok;
}

char* my_strdup(const char *s) {
    if (s == NULL) {
        return NULL;
   }

    size_t length = strlen(s) + 1; // 包括终止符 '\0'
    char *copy = (char *)zmalloc(length);

    if (copy != NULL) {
        memcpy(copy, s, length);
    }

    copy[strlen(s)] = '\0';
    return copy;
}

Error* dirCreateRecursive(const char* path, mode_t mode) {
    char *buf = NULL;
    size_t len;
    Error* status = &Ok;
    int i;
    buf = my_strdup(path);
    if (buf == NULL) {
        return errnoIoCreate("strdup");
    }
    
    len = strlen(buf) + 1;
    for (i = 1; i < len; ++i) {
        if (buf[i] == '/' || buf[i] == '\\') {
            buf[i] = '\0';
            if (mkdir(buf, mode) == -1 && errno != EEXIST) {
                status =  errnoIoCreate("mkdir");
                break;
            }
            buf[i] = '/';
        }
    }
    if (mkdir(buf, mode) == -1 && errno != EEXIST) {
        status =  errnoIoCreate("mkdir");
    }
    zfree(buf);
    return status;
}

int dirIs(char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        // 如果路径不存在或无法访问，返回错误
        return 0;
    }
    // S_ISDIR 宏检查是否为目录
    return S_ISDIR(st.st_mode);
}

#define PATH_MAX 4096
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
  char tmp_path[PATH_MAX];
  while ((pentry = readdir(pdir)) != NULL) {
    if ('.' == pentry->d_name[0]) // 跳过 ./ ..文件和隐藏文件
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
