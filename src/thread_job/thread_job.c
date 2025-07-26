#include "thread_job.h"

void latte_job_init(latte_job_t* job, void* (*run)(int argc, void** args, latte_error_t* error), int argc, void** args) {
    job->run = run;
    job->argc = argc;
    job->args = args;
    job->result = NULL;
    job->error.code = COk;
    job->error.state = NULL;
}


latte_job_t* latte_job_new(void* (*run)(int argc, void** args, latte_error_t* error), int argc, void** args) {
    latte_job_t* job = zmalloc(sizeof(latte_job_t));
    latte_job_init(job, run, argc, args);
    return job;
}

void latte_job_delete(latte_job_t* job) {
    zfree(job);
}

void latte_job_run(latte_job_t* job) {
    job->result = job->run(job->argc, job->args, &job->error);
}