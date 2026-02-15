#ifndef JS_EPOLL_H
#define JS_EPOLL_H

/* ---- struct ---- */

typedef struct {
    int                 fd;          /* epoll fd */
    struct epoll_event *events;      /* reusable array for epoll_wait */
    int                 max_events;
} js_epoll_t;

/* ---- api ---- */

int  js_epoll_init(js_epoll_t *ep, int max_events);
int  js_epoll_add(js_epoll_t *ep, int fd, uint32_t events, void *ptr);
int  js_epoll_mod(js_epoll_t *ep, int fd, uint32_t events, void *ptr);
int  js_epoll_del(js_epoll_t *ep, int fd);
int  js_epoll_wait(js_epoll_t *ep, int timeout_ms);
void js_epoll_free(js_epoll_t *ep);

#endif
