#ifndef LATTE_C_PIDFILE_H
#define LATTE_C_PIDFILE_H
#include "sds/sds.h"

int writePidFile(const char *progName);

//! Cleanup PID file for the current component
/**
 * Removes the PID file for the current component
 *
 */
void removePidFile(void);
const char *getPidPath();
#endif