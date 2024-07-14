#include "../test/testhelp.h"
#include "../test/testassert.h"
#include <stdio.h>
#include "dir.h"
#include <unistd.h>
#include <errno.h>
#include "file.h"
#include "env.h"
#include <dirent.h>   // opendir, readdir, closedir
#include <sys/stat.h> // stat
#include <unistd.h>   // rmdir, unlink
#include <string.h>

// 递归删除目录及其中的所有文件和子目录
void recursive_rmdir(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat st = {};

    // 打开目录
    dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    // 遍历目录中的每个条目
    while ((entry = readdir(dir)) != NULL) {
        char full_path[1024]; // 假定路径长度不超过1024

        // 忽略"."和".."这两个特殊条目
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // 构建完整路径
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // 获取条目状态
        if (stat(full_path, &st) == -1) {
            perror("stat");
            continue;
        }

        // 如果是目录，则递归删除
        if (S_ISDIR(st.st_mode))
            recursive_rmdir(full_path);
        // 如果是文件，则直接删除
        else if (unlink(full_path) == -1) {
            perror("unlink");
        }
    }

    // 关闭目录句柄
    closedir(dir);

    // 最后删除目录本身
    if (rmdir(path) == -1) {
        perror("rmdir");
    }
}
int check_dir_exists(char* dirname) {
    int exists = access(dirname, F_OK);

    if (exists == 0) {
        //存在
        return 1;
    } else if (exists == -1) {
        if (errno == ENOENT) {
            //不存在
            return 0;
        } else {
            //异常
            printf("[check dir exists] %s", strerror(errno));
            return -1;
        }
    }
    return 1;

}

int test_create_dir() {
    assert(check_dir_exists("test_create_dir") == 0);
    Error* error = dirCreate("test_create_dir");
    assert(isOk(error));
    assert(check_dir_exists("test_create_dir") == 1);
    recursive_rmdir("test_create_dir");
    assert(check_dir_exists("test_create_dir") == 0);
    return 1;
}

int test_env_lockfile() {
    Env* env = envCreate();
    sds filename = sdsnew("LOCK");
    FileLock* fileLock;
    Error* error = envLockFile(env, filename, &fileLock);
    assert(isOk(error));
    error = envUnlockFile(env, fileLock);
    assert(isOk(error));
    return 1;
}

int test_env_write_read() {
    recursive_rmdir("test_env_dir");
    Env* env = envCreate();
    Error* error = dirCreate("test_env_dir");
    sds file = sdsnew("test_env_dir/write.log");
    sds read = NULL;
    error = envReadFileToSds(env, file, &read);
    assert(error->code == CNotFound);
    assert(read == NULL);
    sds data = sdsnew("test");
    error = envWriteSdsToFileSync(env, file, data);
    assert(isOk(error));
    error = envReadFileToSds(env, file, &read);
    assert(isOk(error));
    assert(strncmp(read, "test", 4) == 0);
    envRelease(env);

    
    SequentialFile* sf;
    error = sequentialFileCreate(file, &sf);
    
    assert(isOk(error));
    Slice slice = {
        .p = sdsemptylen(100),
        .len = 0
    };
    sds result;
    error = sequentialFileReadSds(sf, 100, &result);
    sequentialFileRelease(sf);
    assert(isOk(error));
    assert(sdslen(result) == 4);
    assert(strncmp("test", result, 4) == 0);
    sdsfree(result);
    
    error = envSequentialFileCreate(env, file, &sf);
    assert(isOk(error));
    error = sequentialFileReadSds(sf, 100, &result);
    sequentialFileRelease(sf);
    assert(isOk(error));
    assert(sdslen(result) == 4);
    assert(strncmp("test", result, 4) == 0);
    sdsfree(result);

    recursive_rmdir("test_env_dir");
    return 1;
}

int test_api(void) {
    {
        #ifdef LATTE_TEST
            // ..... private
        #endif
        test_cond("create dir function", 
            test_create_dir() == 1);
        test_cond("env create function",
            test_env_lockfile() == 1);
        test_cond("env write read function",
            test_env_write_read() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}