#include "js_main.h"

int js_epoll_init(js_epoll_t *ep, int max_events) {
    ep->fd = epoll_create1(EPOLL_CLOEXEC);
    if (ep->fd < 0)
        return -1;
    ep->max_events = max_events;
    ep->events = calloc(max_events, sizeof(struct epoll_event));
    if (!ep->events) {
        close(ep->fd);
        return -1;
    }
    return 0;
}

int js_epoll_add(js_epoll_t *ep, int fd, uint32_t events, void *ptr) {
    struct epoll_event ev = { .events = events, .data.ptr = ptr };
    return epoll_ctl(ep->fd, EPOLL_CTL_ADD, fd, &ev);
}

int js_epoll_mod(js_epoll_t *ep, int fd, uint32_t events, void *ptr) {
    struct epoll_event ev = { .events = events, .data.ptr = ptr };
    return epoll_ctl(ep->fd, EPOLL_CTL_MOD, fd, &ev);
}

int js_epoll_del(js_epoll_t *ep, int fd) {
    return epoll_ctl(ep->fd, EPOLL_CTL_DEL, fd, NULL);
}

int js_epoll_wait(js_epoll_t *ep, int timeout_ms) {
    return epoll_wait(ep->fd, ep->events, ep->max_events, timeout_ms);
}

void js_epoll_free(js_epoll_t *ep) {
    if (ep->fd >= 0)
        close(ep->fd);
    free(ep->events);
    ep->fd = -1;
    ep->events = NULL;
}
