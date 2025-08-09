#ifndef __LATTE_AE_H
#define __LATTE_AE_H
#include "utils/monotonic.h"
#include "log/log.h"
#include "list/list.h"
#include "func_task/func_task.h"

#define AE_OK 0
#define AE_ERR -1

#define AE_NONE 0       /* No events registered. */
#define AE_READABLE 1   /* Fire when descriptor is readable. */
#define AE_WRITABLE 2   /* Fire when descriptor is writable. */
#define AE_BARRIER 4    /* With WRITABLE, never fire the event if the
                           READABLE event already fired in the same event
                           loop iteration. Useful when you want to persist
                           things to disk before sending replies, and want
                           to do that in a group fashion. */

#define AE_FILE_EVENTS (1<<0)
#define AE_TIME_EVENTS (1<<1)
#define AE_ALL_EVENTS (AE_FILE_EVENTS|AE_TIME_EVENTS)
#define AE_DONT_WAIT (1<<2)
#define AE_CALL_BEFORE_SLEEP (1<<3)
#define AE_CALL_AFTER_SLEEP (1<<4)

#define AE_NOMORE -1
#define AE_DELETED_EVENT_ID -1

/* Macros */
#define AE_NOTUSED(V) ((void) V)

struct ae_event_loop_t;

/* Types and data structures */
typedef void ae_file_proc_func(struct ae_event_loop_t *eventLoop, int fd, void *clientData, int mask);
typedef int ae_time_proc_func(struct ae_event_loop_t *eventLoop, long long id, void *clientData);
typedef void ae_event_finalizer_proc_func(struct ae_event_loop_t *eventLoop, void *clientData);
typedef void ae_before_sleep_proc_func(struct ae_event_loop_t *eventLoop);

/* File event structure */
typedef struct ae_file_event_t {
    int mask; /* one of AE_(READABLE|WRITABLE|BARRIER) */
    ae_file_proc_func *rfileProc;
    ae_file_proc_func *wfileProc;
    void *clientData;
} ae_file_event_t;

/* Time event structure */
typedef struct ae_time_event_t {
    long long id; /* time event identifier. */
    monotime when;
    ae_time_proc_func *timeProc;
    ae_event_finalizer_proc_func *finalizerProc;
    void *clientData;
    struct ae_time_event_t *prev;
    struct ae_time_event_t *next;
    int refcount; /* refcount to prevent timer events from being
  		   * freed in recursive time event calls. */
} ae_time_event_t;

/* A fired event */
typedef struct ae_fired_event_t {
    int fd;
    int mask;
} ae_fired_event_t;

/* State of an event based program */
typedef struct ae_event_loop_t {
    int maxfd;   /* highest file descriptor currently registered */
    int setsize; /* max number of file descriptors tracked */
    long long timeEventNextId;
    ae_file_event_t *events; /* Registered events */ //进程句柄fd 对应数组下标
    ae_fired_event_t *fired; /* Fired events */
    ae_time_event_t *timeEventHead;
    int stop;
    void *apidata; /* This is used for polling API specific data */
    ae_before_sleep_proc_func *beforesleep;
    list_t* beforesleeps;
    ae_before_sleep_proc_func *aftersleep;
    list_t* aftersleeps;
    int flags;
    void *privdata[2];
} ae_event_loop_t;



/* file event */
int ae_file_event_new(ae_event_loop_t *eventLoop, int fd, int mask,
        ae_file_proc_func *proc, void *clientData);
void ae_file_event_delete(ae_event_loop_t *eventLoop, int fd, int mask);
int ae_file_event_get_mask(ae_event_loop_t *eventLoop, int fd);
/* time event */
long long ae_time_event_new(ae_event_loop_t *eventLoop, long long milliseconds,
        ae_time_proc_func *proc, void *clientData,
        ae_event_finalizer_proc_func *finalizerProc);
int ae_time_event_delete(ae_event_loop_t *eventLoop, long long id);
int ae_process_events(ae_event_loop_t *eventLoop, int flags);

/* Prototypes */
ae_event_loop_t *ae_event_loop_new(int setsize);
void ae_event_loop_delete(ae_event_loop_t *eventLoop);

int ae_wait(int fd, int mask, long long milliseconds);
void ae_main(ae_event_loop_t *eventLoop);
void ae_stop(ae_event_loop_t *eventLoop);
char *ae_get_api_name(void);
void ae_set_before_sleep_proc(ae_event_loop_t *eventLoop, ae_before_sleep_proc_func *beforesleep);
void ae_add_before_sleep_task(ae_event_loop_t* eventLoop, latte_func_task_t* task);
void ae_set_after_sleep_proc(ae_event_loop_t *eventLoop, ae_before_sleep_proc_func *aftersleep);
int ae_get_set_size(ae_event_loop_t *eventLoop);
int ae_resize_set_size(ae_event_loop_t *eventLoop, int setsize);
void ae_set_dont_wait(ae_event_loop_t *eventLoop, int noWait);


void ae_do_before_sleep(ae_event_loop_t *eventLoop);
int ae_read(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len);
int ae_write(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len);

/* api 需要实现*/
// return -1 失败 0 成功
// static int ae_api_create(ae_event_loop_t *eventLoop);
// return -1 失败 0 成功
// static int ae_api_resize(ae_event_loop_t *eventLoop, int setsize);
// static void ae_api_delete(ae_event_loop_t *eventLoop);
// static int ae_api_add_event(ae_event_loop_t *eventLoop, int fd, int mask);
// static void ae_api_del_event(ae_event_loop_t *eventLoop, int fd, int delmask);
// static int ae_api_poll(ae_event_loop_t *eventLoop, struct timeval *tvp);
// static char *ae_api_name(void);
// static int ae_api_read(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len);
// static int ae_api_write(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len);
// static void ae_api_before_sleep(ae_event_loop_t *eventLoop);
// static void ae_api_after_sleep(ae_event_loop_t *eventLoop);
#endif