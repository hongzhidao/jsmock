#include "js_main.h"

/* ---- buf ---- */

void js_buf_init(js_buf_t *buf) {
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

int js_buf_append(js_buf_t *buf, const char *data, size_t len) {
    if (buf->len + len > buf->cap) {
        size_t newcap = buf->cap ? buf->cap * 2 : 1024;
        while (newcap < buf->len + len)
            newcap *= 2;
        char *p = realloc(buf->data, newcap);
        if (!p)
            return -1;
        buf->data = p;
        buf->cap = newcap;
    }
    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
    return 0;
}

void js_buf_consume(js_buf_t *buf, size_t n) {
    if (n >= buf->len) {
        buf->len = 0;
        return;
    }
    memmove(buf->data, buf->data + n, buf->len - n);
    buf->len -= n;
}

void js_buf_free(js_buf_t *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

/* ---- conn ---- */

js_conn_t *js_conn_create(int fd) {
    js_conn_t *conn = calloc(1, sizeof(*conn));
    if (!conn)
        return NULL;
    conn->fd = fd;
    conn->state = JS_CONN_READING;
    js_buf_init(&conn->rbuf);
    js_buf_init(&conn->wbuf);
    conn->woff = 0;
    conn->last_active = time(NULL);
    return conn;
}

int js_conn_read(js_conn_t *conn) {
    char tmp[4096];
    ssize_t n = read(conn->fd, tmp, sizeof(tmp));
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
    ssize_t n = write(conn->fd, conn->wbuf.data + conn->woff, remaining);
    if (n < 0)
        return -1;
    conn->woff += n;
    conn->last_active = time(NULL);
    /* return 1 if fully written, 0 if more to go */
    return (conn->woff >= conn->wbuf.len) ? 1 : 0;
}

void js_conn_close(js_conn_t *conn, js_epoll_t *ep) {
    js_epoll_del(ep, conn->fd);
    close(conn->fd);
    conn->state = JS_CONN_CLOSING;
}

void js_conn_free(js_conn_t *conn) {
    if (!conn)
        return;
    js_buf_free(&conn->rbuf);
    js_buf_free(&conn->wbuf);
    free(conn);
}
