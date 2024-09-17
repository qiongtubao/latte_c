#ifndef LATTE_C_PROCESS_H
#define LATTE_C_PROCESS_H
#include "sds/sds.h"
#include "log/log.h"
sds get_process_name(const char *prog_full_name);
int daemonize_service(sds std_out_file, sds std_err_file);
void sys_log_redirect(const char *std_out_file, const char *std_err_file);
#endif