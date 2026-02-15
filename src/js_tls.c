#include "js_main.h"

/* TLS support - placeholder stubs */

int js_tls_init(js_tls_t *tls, const char *cert_path, const char *key_path) {
    (void)tls; (void)cert_path; (void)key_path;
    return -1; /* not implemented */
}

int js_tls_accept(js_tls_t *tls, js_conn_t *conn) {
    (void)tls; (void)conn;
    return -1;
}

int js_tls_read(js_conn_t *conn, char *buf, size_t len) {
    (void)conn; (void)buf; (void)len;
    return -1;
}

int js_tls_write(js_conn_t *conn, const char *buf, size_t len) {
    (void)conn; (void)buf; (void)len;
    return -1;
}

void js_tls_free(js_tls_t *tls) {
    (void)tls;
}
