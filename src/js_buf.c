#include "js_main.h"

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
