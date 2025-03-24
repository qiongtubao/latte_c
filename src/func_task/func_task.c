#include "func_task.h"
#include <stdarg.h>
#include "zmalloc/zmalloc.h"

latte_func_task_t* latte_func_task_new(task_fn* exec, callback_fn* cb,int argv, ...) {
    va_list valist;
    /* Allocate memory for the job structure and all required
     * arguments */
    latte_func_task_t *task = zmalloc(sizeof(latte_func_task_t));
    task->exec = exec;
    task->cb = cb;
    task->argv = argv;
    task->args = zmalloc(sizeof(void*) * argv);
    va_start(valist, argv);
    for (int i = 0; i < argv; i++) {
        task->args[i] = va_arg(valist, void *);
    }
    va_end(valist);
    return task;

}

void exec_task(latte_func_task_t* task) {
    task->exec(task);
}
void callback_task(latte_func_task_t* task) {
    if (task->cb) task->cb(task);
}