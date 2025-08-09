#include "ae.h"
#include <sys/select.h>
#include <string.h>

typedef struct ae_api_state_t {
    fd_set rfds, wfds;
    /* We need to have a copy of the fd sets as it's not safe to reuse
     * FD sets after select(). */
    fd_set _rfds, _wfds;
} ae_api_state_t;

// return -1 失败 0 成功
static int ae_api_create(ae_event_loop_t *eventLoop) {
    LATTE_LIB_LOG(LOG_DEBUG, "[ae_api_create] ae use select");
    ae_api_state_t *state = zmalloc(sizeof(ae_api_state_t));

    if (!state) return -1;
    FD_ZERO(&state->rfds);
    FD_ZERO(&state->wfds);
    eventLoop->apidata = state;
    return 0;
}
//return -1 失败 0 成功
static int ae_api_resize(ae_event_loop_t *eventLoop, int setsize) {
    /* Just ensure we have enough room in the fd_set type. */
    if (setsize >= FD_SETSIZE) return -1;
    return 0;
}
static void ae_api_delete(ae_event_loop_t *eventLoop) {
    zfree(eventLoop->apidata);
}
static int ae_api_add_event(ae_event_loop_t *eventLoop, int fd, int mask) {
    ae_api_state_t *state = eventLoop->apidata;

    if (mask & AE_READABLE) FD_SET(fd,&state->rfds);
    if (mask & AE_WRITABLE) FD_SET(fd,&state->wfds);
    return 0;
}
static void ae_api_del_event(ae_event_loop_t *eventLoop, int fd, int delmask) {
    ae_api_state_t *state = eventLoop->apidata;

    if (delmask & AE_READABLE) FD_CLR(fd,&state->rfds);
    if (delmask & AE_WRITABLE) FD_CLR(fd,&state->wfds);
}
static int ae_api_poll(ae_event_loop_t *eventLoop, struct timeval *tvp) {
    ae_api_state_t *state = eventLoop->apidata;
    int retval, j, numevents = 0;

    memcpy(&state->_rfds,&state->rfds,sizeof(fd_set));
    memcpy(&state->_wfds,&state->wfds,sizeof(fd_set));

    retval = select(eventLoop->maxfd+1,
                &state->_rfds,&state->_wfds,NULL,tvp);
    if (retval > 0) {
        for (j = 0; j <= eventLoop->maxfd; j++) {
            int mask = 0;
            ae_file_event_t *fe = &eventLoop->events[j];

            if (fe->mask == AE_NONE) continue;
            if (fe->mask & AE_READABLE && FD_ISSET(j,&state->_rfds))
                mask |= AE_READABLE;
            if (fe->mask & AE_WRITABLE && FD_ISSET(j,&state->_wfds))
                mask |= AE_WRITABLE;
            eventLoop->fired[numevents].fd = j;
            eventLoop->fired[numevents].mask = mask;
            numevents++;
        }
    }
    return numevents;
}
static char *ae_api_name(void) {
    return "select";
}
static int ae_api_read(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {
    AE_NOTUSED(eventLoop);
    return read(fd, buf, buf_len);
}
static int ae_api_write(ae_event_loop_t *eventLoop, int fd, void *buf, size_t buf_len) {
    AE_NOTUSED(eventLoop);
    return write(fd, buf, buf_len);
}
static void ae_api_before_sleep(ae_event_loop_t *eventLoop) {
    AE_NOTUSED(eventLoop);
}
static void ae_api_after_sleep(ae_event_loop_t *eventLoop) {
    AE_NOTUSED(eventLoop);
}