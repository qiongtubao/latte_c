#include "pidfile.h"
#include "process.h"
#include "latte_signal.h"
#include "test/testhelp.h"
#include "test/testassert.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *quit_thread_func(void *_signum)
{
  intptr_t signum = (intptr_t)_signum;
  LATTE_LIB_LOG(LOG_INFO,"Receive signal: %ld", signum);
  return NULL;
}

void quit_signal_handle(int signum)
{
  // 防止多次调用退出
  // 其实正确的处理是，应该全局性的控制来防止出现“多次”退出的状态，包括发起信号
  // 退出与进程主动退出
  set_signal_handler(NULL);

  pthread_t tid;
  pthread_create(&tid, NULL, quit_thread_func, (void *)(intptr_t)signum);
}

int test_signal() {
    set_signal_handler(quit_signal_handle);
    sleep(1);
    return 1;
}

// 解析字符串为浮点数
bool str_to_double(const char *str, double *val) {
    char *endptr;
    double temp_val = strtod(str, &endptr);

    // 检查是否解析成功
    if (endptr == str || *endptr != '\0') {
        // 解析失败
        *val = 0.0;
        return false;
    } else {
        // 解析成功
        *val = temp_val;
        return true;
    }
}

// 解析字符串为 unsigned long long 类型的整数
bool str_to_ullonglong(const char *str, unsigned long long *val, int base /* = 10 */) {
    char *endptr;
    unsigned long long temp_val = strtoull(str, &endptr, base);

    // 检查是否解析成功
    if (endptr == str || *endptr != '\0') {
        // 解析失败
        *val = 0;
        return false;
    } else {
        // 解析成功
        *val = temp_val;
        return true;
    }
}




int readFromFile(const char* fileName, char** outputData, size_t* fileSize)
{
  FILE *file = fopen(fileName, "rb");
  if (file == NULL) {
    LATTE_LIB_LOG(LOG_ERROR, "Failed to open file %s" ,fileName);
    return -1;
  }

  // fseek( file, 0, SEEK_END );
  // size_t fsSize = ftell( file );
  // fseek( file, 0, SEEK_SET );

  char   buffer[4 * 1024U];
  size_t readSize = 0;
  size_t oneRead  = 0;

  char *data = NULL;
  do {
    memset(buffer, 0, sizeof(buffer));
    oneRead = fread(buffer, 1, sizeof(buffer), file);
    if (ferror(file)) {
      LATTE_LIB_LOG(LOG_ERROR, "Failed to read data %s" , fileName);
      fclose(file);
      if (data != NULL) {
        free(data);
        data = NULL;
      }
      return -1;
    }

    data = (char *)realloc(data, readSize + oneRead);
    if (data == NULL) {
      LATTE_LIB_LOG(LOG_ERROR,"Failed to alloc memory for %s" ,fileName);
      free(data);
      fclose(file);
      return -1;
    } else {
      memcpy(data + readSize, buffer, oneRead);
      readSize += oneRead;
    }

  } while (feof(file) == 0);

  fclose(file);

  data           = (char *)realloc(data, readSize + 1);
  data[readSize] = '\0';
  *outputData     = data;
  *fileSize       = readSize;
  return 0;
}

int test_pid_file() {
  long long pid = (long long)getpid();

  const char *programName = "test";
  writePidFile(programName);

  char* pidFile = getPidPath();

  char  *p    = NULL;
  size_t size = 0;
  readFromFile(pidFile, &p, &size);

  char*    temp = (p);
  long long target = 0;
  str_to_ullonglong(temp, &target, 10);

  free(p);

  assert(pid == target);
}
int test_api(void) {
    {
#ifdef LATTE_TEST
        // ..... private
#endif
        test_cond("signal function",
                  test_signal() == 1);

    } test_report()
    return 1;
}

int main() {
    test_api();
    return 0;
}