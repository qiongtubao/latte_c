

#ifndef __LATTE_THREAD_JOB_H
#define __LATTE_THREAD_JOB_H

#include "error/error.h"
#include "utils/atomic.h"
#include "utils/config.h"

typedef struct latte_job_t {
    void* (*run)(int argc, void** args, latte_error_t* error);
    void* result;
    int argc;
    void** args;
    latte_error_t error;
} latte_job_t;

latte_job_t* latte_job_new(void* (*run)(int argc, void** args, latte_error_t* error), int argc, void** args);
void latte_job_init(latte_job_t* job, void* (*run)(int argc, void** args, latte_error_t* error), int argc, void** args);
void latte_job_delete(latte_job_t* job);
void latte_job_run(latte_job_t* job);



#endif