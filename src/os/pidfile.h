#ifndef LATTE_C_PIDFILE_H
#define LATTE_C_PIDFILE_H
#include "sds/sds.h"

int write_pid_file(const char *progName);

//! Cleanup PID file for the current component
/**
 * Removes the PID file for the current component
 *
 */
void remove_pid_file(void);
const char* get_pid_file();
#endif