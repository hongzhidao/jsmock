#ifndef JS_BUF_H
#define JS_BUF_H

/* ---- struct ---- */

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} js_buf_t;

/* ---- api ---- */

void js_buf_init(js_buf_t *buf);
int  js_buf_append(js_buf_t *buf, const char *data, size_t len);
void js_buf_consume(js_buf_t *buf, size_t n);
void js_buf_free(js_buf_t *buf);

#endif
