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
#include "fs.h"
#include <fcntl.h>

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


    assert(dirIs("test_create_dir") == 0);
    error = dirCreateRecursive("test_create_dir", 0755);
    assert(isOk(error));
    assert(dirIs("test_create_dir") == 1);
    recursive_rmdir("test_create_dir");
    assert(dirIs("test_create_dir") == 0);
    return 1;
}

int test_env_lockfile() {
    Env* env = envCreate();
    sds_t filename = sds_new("LOCK");
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
    sds_t file = sds_new("test_env_dir/write.log");
    sds_t read = NULL;
    error = envReadFileToSds(env, file, &read);
    assert(error->code == CNotFound);
    assert(read == NULL);
    sds_t data = sds_new("test");
    error = envWriteSdsToFileSync(env, file, data);
    assert(isOk(error));
    error = envReadFileToSds(env, file, &read);
    assert(isOk(error));
    assert(strncmp(read, "test", 4) == 0);
    //double free
    envRelease(env);

    
    SequentialFile* sf;
    error = sequentialFileCreate(file, &sf);
    
    assert(isOk(error));
    // slice_t slice = {
    //     .p = sds_empty_len(100),
    //     .len = 0
    // };
    sds_t result;
    error = sequentialFileReadSds(sf, 100, &result);
    sequentialFileRelease(sf);
    assert(isOk(error));
    assert(sds_len(result) == 4);
    assert(strncmp("test", result, 4) == 0);
    sds_delete(result);
    
    error = envSequentialFileCreate(env, file, &sf);
    assert(isOk(error));
    error = sequentialFileReadSds(sf, 100, &result);
    sequentialFileRelease(sf);
    assert(isOk(error));
    assert(sds_len(result) == 4);
    assert(strncmp("test", result, 4) == 0);
    sds_delete(result);

    recursive_rmdir("test_env_dir");
    return 1;
}

int test_fs() {
    recursive_rmdir("test_fs");
    assert(isOk(dirCreate("test_fs")));
    int fd = open("test_fs/test.txt", O_CREAT | O_RDWR , 0644);
    assert(file_exists("test_fs/test.txt"));
    char buf[10];
    int size = 0;
    int ret = readn(fd, buf, 10, &size);
    assert(ret == -1);
    assert(size == 0);
    
    ret = writen(fd, "hello", 5);
    assert(ret == 0);

    assert(lseek(fd, 0, SEEK_SET) != -1);
    ret = readn(fd, buf, 10, &size);
    assert(ret == -1);
    assert(size == 5);
    assert(strncmp(buf, "hello", 5) == 0);

    assert(lseek(fd, 0, SEEK_SET) != -1);
    ret = writen(fd, "hello_world", 11);
    assert(ret == 0);

    assert(lseek(fd, 0, SEEK_SET) != -1);
    size = 0;
    ret = readn(fd, buf, 10, &size);
    assert(ret == 0);
    assert(size == 10);
    assert(strncmp(buf, "hello_world", 11) != 0);
    assert(strncmp(buf, "hello_world", 10) == 0);

    assert(lseek(fd, 0, SEEK_SET) != -1);
    sds_t result = readall(fd);
    assert(result != NULL);
    assert(sds_len(result) == 11);
    assert(strncmp(result, "hello_world", 11) == 0);

    recursive_rmdir("test_fs");
    return 1;
}

int test_dir() {
    assert(dirIs("./test_dir") == 1);
    latte_iterator_t* iter = dir_scan_file("./test_dir", ".*\\.txt$");
    int i = 0;
    
    while(latte_iterator_has_next(iter)) {
        sds_t file = (sds)latte_iterator_next(iter);
        assert(file != NULL);
        i++;
    }
    //a.txt b.txt
    assert(i == 2);
    
    latte_iterator_delete(iter);

    iter = dir_scan_file("./test_dir", ".*\\.t$");
    while(latte_iterator_has_next(iter)) {
        sds_t file = latte_iterator_next(iter);
        assert(sds_cmp(file, "a.t"));
    }
    latte_iterator_delete(iter);

    iter = dir_scan_file("./test_dir1", ".*\\.txt$");
    assert(iter == NULL);
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
        test_cond("fs function",
            test_fs() == 1);
        test_cond("dir function",
            test_dir() == 1);
    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}