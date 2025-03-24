

#ifndef __FUNC_TASK_H
#define __FUNC_TASK_H

typedef struct latte_func_task_t latte_func_task_t;
typedef int task_fn(latte_func_task_t* task);
typedef void callback_fn(latte_func_task_t* task);

typedef struct latte_func_task_t {
    int argv;
    void** args;
    void* result;
    int error;
    int id;
    task_fn *exec;
    callback_fn *cb;
} latte_func_task_t;

latte_func_task_t* latte_func_task_new(task_fn* exec, callback_fn* cb,int argv, ...);
void exec_task(latte_func_task_t*);
void callback_task(latte_func_task_t*);


#endif