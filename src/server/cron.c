#include "cron.h"
#include "zmalloc/zmalloc.h"

cron_t* cron_new(exec_cron_func fn, long long period) {
    cron_t* cron = (cron_t*)zmalloc(sizeof(cron_t));
    if (cron == NULL) {
        return NULL;
    }
    cron->period = period;
    cron->fn = fn;
    return cron;
}

void cron_delete(cron_t*cron) {
    if (cron == NULL) {
        return;
    }
    zfree(cron);
}

cron_manager_t* cron_manager_new(void) {
    cron_manager_t* cron_manager = (cron_manager_t*)zmalloc(sizeof(cron_manager_t));
    if (cron_manager == NULL) {
        return NULL;
    }
    cron_manager->cron_loops = 0;
    cron_manager->crons = vector_new();
    if (cron_manager->crons == NULL) {
        zfree(cron_manager);
        return NULL;
    }
    return cron_manager;
}

void cron_manager_delete(cron_manager_t*cron_manager) {
    if (cron_manager == NULL) {
        return;
    }
    for (int i = 0; i < vector_size(cron_manager->crons); i++) {
        cron_t* cron = (cron_t*)vector_get(cron_manager->crons, i);
        cron_delete(cron);
    }
    vector_delete(cron_manager->crons);
    zfree(cron_manager);
}

void cron_manager_add_cron(cron_manager_t* cron_manager, cron_t*cron) {
    if (cron_manager == NULL) {
        return;
    }
    vector_add(cron_manager->crons, cron);
}

void cron_manager_remove_cron(cron_manager_t*cron_manager, cron_t*cron) {
    if (cron_manager == NULL) {
        return;
    }
    vector_remove(cron_manager->crons, cron);
}

void cron_manager_run(cron_manager_t*cron_manager, void* arg) {
    if (cron_manager == NULL) {
        return;
    }
    vector_t* crons = cron_manager->crons;
    for (int i = 0; i < vector_size(crons); i++) {
        cron_t* cron = (cron_t*)vector_get(crons, i);
        if (cron == NULL || cron->fn == NULL) {
            continue;
        }
        if (cron_manager->cron_loops % cron->period == 0) {
            cron->fn(arg);
        }
    }
    cron_manager->cron_loops++;
}
