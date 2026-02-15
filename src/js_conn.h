#ifndef JS_CONN_H
#define JS_CONN_H

/* ---- struct ---- */

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} js_buf_t;

typedef enum {
    JS_CONN_READING,
    JS_CONN_WRITING,
    JS_CONN_CLOSING
} js_conn_state_t;

typedef struct {
    int              fd;
    js_conn_state_t  state;
    js_buf_t         rbuf;
    js_buf_t         wbuf;
    size_t           woff;          /* write offset (bytes already sent) */
    time_t           last_active;
} js_conn_t;

/* ---- buf api ---- */

void js_buf_init(js_buf_t *buf);
int  js_buf_append(js_buf_t *buf, const char *data, size_t len);
void js_buf_consume(js_buf_t *buf, size_t n);
void js_buf_free(js_buf_t *buf);

/* ---- conn api ---- */

js_conn_t *js_conn_create(int fd);
int        js_conn_read(js_conn_t *conn);
int        js_conn_write(js_conn_t *conn);
void       js_conn_close(js_conn_t *conn, js_epoll_t *ep);
void       js_conn_free(js_conn_t *conn);

#endif
