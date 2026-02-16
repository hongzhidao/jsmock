#include "js_main.h"

/* ---- listen ---- */

static void js_conn_on_read(js_event_t *ev);
static void js_conn_on_write(js_event_t *ev);

static void js_listen_accept(js_event_t *ev) {
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

    conn->event.read = js_conn_on_read;
    conn->event.write = js_conn_on_write;

    js_epoll_add(&eng->epoll, fd, EPOLLIN, &conn->event);
}

int js_listen_start(js_event_t *ev, int lfd, js_epoll_t *ep)
{
    ev->fd = lfd;
    ev->read = js_listen_accept;
    ev->write = NULL;
    return js_epoll_add(ep, lfd, EPOLLIN | EPOLLEXCLUSIVE, ev);
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

/* ---- conn event handlers ---- */

static void js_conn_on_read(js_event_t *ev) {
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

static void js_conn_on_write(js_event_t *ev) {
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
