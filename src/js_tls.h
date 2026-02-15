#ifndef JS_TLS_H
#define JS_TLS_H

/* ---- struct ---- */

typedef struct {
    void *ctx;   /* SSL_CTX*, opaque for now */
} js_tls_t;

/* ---- api (placeholder) ---- */

int   js_tls_init(js_tls_t *tls, const char *cert_path, const char *key_path);
int   js_tls_accept(js_tls_t *tls, js_conn_t *conn);
int   js_tls_read(js_conn_t *conn, char *buf, size_t len);
int   js_tls_write(js_conn_t *conn, const char *buf, size_t len);
void  js_tls_free(js_tls_t *tls);

#endif
