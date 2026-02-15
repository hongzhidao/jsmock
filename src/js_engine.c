#include "js_main.h"
#include <fcntl.h>
#include <errno.h>

/* tag pointers to distinguish special fds in epoll */
static char js_engine_tag_listen;
static char js_engine_tag_timer;

#define JS_TAG_LISTEN ((void *)&js_engine_tag_listen)
#define JS_TAG_TIMER  ((void *)&js_engine_tag_timer)

int js_engine_init(js_engine_t *eng, int max_events) {
    if (js_epoll_init(&eng->epoll, max_events) < 0)
        return -1;
    if (js_timer_init(&eng->timer) < 0) {
        js_epoll_free(&eng->epoll);
        return -1;
    }
    /* register timerfd in epoll */
    js_epoll_add(&eng->epoll, eng->timer.tfd, EPOLLIN, JS_TAG_TIMER);
    return 0;
}

static void js_engine_accept(js_engine_t *eng, int lfd) {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int fd = accept4(lfd, (struct sockaddr *)&addr, &addrlen,
                     SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (fd < 0)
        return;

    js_conn_t *conn = js_conn_create(fd);
    if (!conn) {
        close(fd);
        return;
    }
    js_epoll_add(&eng->epoll, fd, EPOLLIN, conn);
}

static void js_engine_handle_conn(js_engine_t *eng, js_conn_t *conn,
                                  uint32_t events) {
    if (events & EPOLLIN) {
        int rc = js_conn_read(conn);
        if (rc <= 0) {
            js_conn_close(conn, &eng->epoll);
            js_conn_free(conn);
            return;
        }

        /* try to parse a complete HTTP request */
        js_http_request_t req = {0};
        int parsed = js_http_parse_request(&conn->rbuf, &req);
        if (parsed < 0) {
            js_conn_close(conn, &eng->epoll);
            js_conn_free(conn);
            return;
        }
        if (parsed == 0)
            return; /* need more data */

        /* execute JS handler */
        js_http_response_t resp = {0};
        js_runtime_t *rt = js_thread_current->rt;
        if (js_qjs_handle_request(rt, &req, &resp) < 0) {
            resp.status = 500;
            resp.body = strdup("Internal Server Error");
            resp.body_len = strlen(resp.body);
        }
        js_http_request_free(&req);

        /* serialize response into write buffer */
        js_http_serialize_response(&resp, &conn->wbuf);
        js_http_response_free(&resp);

        conn->state = JS_CONN_WRITING;
        js_epoll_mod(&eng->epoll, conn->fd, EPOLLOUT, conn);
    }

    if (events & EPOLLOUT) {
        int rc = js_conn_write(conn);
        if (rc < 0) {
            js_conn_close(conn, &eng->epoll);
            js_conn_free(conn);
            return;
        }
        if (rc == 1) {
            /* fully written, close connection (no keep-alive for now) */
            js_conn_close(conn, &eng->epoll);
            js_conn_free(conn);
        }
    }
}

void js_engine_run(js_engine_t *eng) {
    js_runtime_t *rt = js_thread_current->rt;
    int lfd = rt->lfd;

    /* add listen fd with EPOLLEXCLUSIVE to avoid thundering herd */
    js_epoll_add(&eng->epoll, lfd, EPOLLIN | EPOLLEXCLUSIVE, JS_TAG_LISTEN);

    for (;;) {
        int n = js_epoll_wait(&eng->epoll, 1000);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }

        for (int i = 0; i < n; i++) {
            struct epoll_event *ev = &eng->epoll.events[i];
            void *ptr = ev->data.ptr;

            if (ptr == JS_TAG_LISTEN) {
                js_engine_accept(eng, lfd);
            } else if (ptr == JS_TAG_TIMER) {
                js_timer_sweep(&eng->timer);
            } else {
                js_conn_t *conn = ptr;
                js_engine_handle_conn(eng, conn, ev->events);
            }
        }
    }
}

void js_engine_free(js_engine_t *eng) {
    js_timer_free(&eng->timer);
    js_epoll_free(&eng->epoll);
}
