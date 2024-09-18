#include "process.h"
#include <string.h>
#include "log/log.h"
#include "utils/utils.h"
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils/utils.h"


char *basename(const char *path) {
    const char *slash = strrchr(path, '/');
    if (slash != NULL) {
        return (char *)(slash + 1);
    } else {
        // 如果没有斜杠，则返回整个路径
        return (char *)path;
    }
}

sds get_process_name(const char *prog_name) {
  sds process_name;
  int buf_len = strlen(prog_name);
//   assert(buf_len);
  char buf[buf_len + 1];
  if (buf == NULL) {
    log_error("latte_lib-os","Failed to alloc memory for program name.");
    return "";
  }
  snprintf(buf, buf_len + 1, "%s", prog_name);  // 第二个参数需为buf_len + 1

  process_name = basename(buf);
  return sdsnew(process_name);
}


int daemonize_service0(bool close_std_streams) {
    int nochdir = 1;
    int noclose = close_std_streams ? 0 : 1;
#ifdef __MACH__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  int rc = daemon(nochdir, noclose);
#ifdef __MACH__
#pragma GCC diagnostic pop
#endif
    // Here after the fork; the parent is dead and setsid() is called
  if (rc != 0) {
    log_error("latte_lib", "Error: unable to daemonize: %s \n" ,strerror(errno));
  }
  return rc;
}

#define RWRR (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
void sys_log_redirect(const char *std_out_file, const char *std_err_file)
{
  int rc = 0;

  // Redirect stdin to /dev/null
  int nullfd = open("/dev/null", O_RDONLY);
  if (nullfd >= 0) {
    dup2(nullfd, STDIN_FILENO);
    close(nullfd);
  }

  // Get timestamp.
  struct timeval tv;
  rc = gettimeofday(&tv, NULL);
  if (rc != 0) {
    log_error("latte_lib", "Fail to get current time");
    tv.tv_sec = 0;
  }

  int std_err_flag, std_out_flag;
  // Always use append-write. And if not exist, create it.
  std_err_flag = std_out_flag = O_CREAT | O_APPEND | O_WRONLY;

  sds err_file = getAbsolutePath(std_err_file);

  // CWE367: A check occurs on a file's attributes before the file is
  // used in a privileged operation, but things may have changed
  // Redirect stderr to std_err_file
  // struct stat st;
  // rc = stat(err_file.c_str(), &st);
  // if (rc != 0 || st.st_size > MAX_ERR_OUTPUT) {
  //   // file may not exist or oversize
  //   std_err_flag |= O_TRUNC; // Remove old content if any.
  // }

  std_err_flag |= O_TRUNC;  // Remove old content if any.

  int errfd = open(err_file, std_err_flag, RWRR);
  if (errfd >= 0) {
    dup2(errfd, STDERR_FILENO);
    close(errfd);
  }
  setvbuf(stderr, NULL, _IONBF, 0);  // Make sure stderr is not buffering
  log_error("latte_lib", "Process %d built error output at %lld ", getpid() , tv.tv_sec );

  sds outFile = getAbsolutePath(std_out_file);

  // Redirect stdout to outFile.c_str()
  // rc = stat(outFile.c_str(), &st);
  // if (rc != 0 || st.st_size > MAX_STD_OUTPUT) {
  //   // file may not exist or oversize
  //   std_out_flag |= O_TRUNC; // Remove old content if any.
  // }

  std_out_flag |= O_TRUNC;  // Remove old content if any.
  int outfd = open(outFile, std_out_flag, RWRR);
  if (outfd >= 0) {
    dup2(outfd, STDOUT_FILENO);
    close(outfd);
  }
  setvbuf(stdout, NULL, _IONBF, 0);  // Make sure stdout not buffering
  log_error("latte_lib", "Process %d  built standard output at %lld", getpid(),  tv.tv_sec);
  return;
}


int daemonize_service(sds stdout, sds stderr) {
    int rc = daemonize_service0(false);
    if (rc != 0) {
        log_error("latte_lib", "Error: \n");
        return rc;
    }
    sys_log_redirect(stdout, stderr);
    return 0;
}

