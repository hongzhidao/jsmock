#include "js_main.h"

/* ---- listen ---- */

static void js_listen_accept(js_event_t *ev) {
    js_listen_t *ls = js_event_data(ev, js_listen_t, event);
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

    ls->on_conn_init(conn);
    js_epoll_add(&eng->epoll, fd, EPOLLIN, &conn->event);
}

int js_listen_start(js_listen_t *ls, int lfd, js_epoll_t *ep,
                    js_conn_init_t on_conn_init)
{
    ls->event.fd = lfd;
    ls->event.read = js_listen_accept;
    ls->event.write = NULL;
    ls->on_conn_init = on_conn_init;
    return js_epoll_add(ep, lfd, EPOLLIN | EPOLLEXCLUSIVE, &ls->event);
}

/* ---- conn ---- */

js_conn_t *js_conn_create(int fd) {
    js_conn_t *conn = calloc(1, sizeof(*conn));
    if (!conn)
        return NULL;
    conn->event.fd = fd;
    conn->state = JS_CONN_READING;
    js_buf_init(&conn->rbuf);
    js_buf_init(&conn->wbuf);
    conn->woff = 0;
    conn->last_active = time(NULL);
    return conn;
}

int js_conn_read(js_conn_t *conn) {
    char tmp[4096];
    ssize_t n = read(conn->event.fd, tmp, sizeof(tmp));
    if (n <= 0)
        return (int)n; /* 0 = EOF, -1 = error */
    conn->last_active = time(NULL);
    if (js_buf_append(&conn->rbuf, tmp, n) < 0)
        return -1;
    return (int)n; /* positive = bytes read */
}

int js_conn_write(js_conn_t *conn) {
    size_t remaining = conn->wbuf.len - conn->woff;
    if (remaining == 0)
        return 0;
    ssize_t n = write(conn->event.fd, conn->wbuf.data + conn->woff, remaining);
    if (n < 0)
        return -1;
    conn->woff += n;
    conn->last_active = time(NULL);
    /* return 1 if fully written, 0 if more to go */
    return (conn->woff >= conn->wbuf.len) ? 1 : 0;
}

void js_conn_close(js_conn_t *conn, js_epoll_t *ep) {
    js_epoll_del(ep, conn->event.fd);
    close(conn->event.fd);
    conn->state = JS_CONN_CLOSING;
}

void js_conn_free(js_conn_t *conn) {
    if (!conn)
        return;
    js_buf_free(&conn->rbuf);
    js_buf_free(&conn->wbuf);
    free(conn);
}

