#include "js_main.h"
#include <fcntl.h>

static js_msec_t js_engine_time(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (js_msec_t) (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int js_engine_init(js_engine_t *eng, int max_events) {
    if (js_epoll_init(&eng->epoll, max_events) < 0)
        return -1;
    js_timers_init(&eng->timers);
    return 0;
}

static void js_engine_conn_read(js_event_t *ev);
static void js_engine_conn_write(js_event_t *ev);

void js_engine_accept(js_event_t *ev) {
    js_engine_t *eng = &js_thread_current->engine;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int fd = accept4(ev->fd, (struct sockaddr *)&addr, &addrlen,
                     SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (fd < 0)
        return;

    js_conn_t *conn = js_conn_create(fd);
    if (!conn) {
        close(fd);
        return;
    }

    conn->event.read = js_engine_conn_read;
    conn->event.write = js_engine_conn_write;

    js_epoll_add(&eng->epoll, fd, EPOLLIN, &conn->event);
}

static void js_engine_conn_read(js_event_t *ev) {
    js_engine_t *eng = &js_thread_current->engine;
    js_conn_t *conn = js_event_data(ev, js_conn_t, event);

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
    js_epoll_mod(&eng->epoll, ev->fd, EPOLLOUT, ev);
}

static void js_engine_conn_write(js_event_t *ev) {
    js_engine_t *eng = &js_thread_current->engine;
    js_conn_t *conn = js_event_data(ev, js_conn_t, event);

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

void js_engine_run(js_engine_t *eng) {
    js_msec_t timeout;

    for (;;) {
        timeout = js_timer_find(&eng->timers);

        if (js_epoll_poll(&eng->epoll, (int) timeout) < 0)
            break;

        js_timer_expire(&eng->timers, js_engine_time());
    }
}

void js_engine_free(js_engine_t *eng) {
    js_epoll_free(&eng->epoll);
}
