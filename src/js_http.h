#ifndef JS_HTTP_H
#define JS_HTTP_H

/* ---- enum ---- */

typedef enum {
    JS_HTTP_GET,
    JS_HTTP_POST,
    JS_HTTP_PUT,
    JS_HTTP_PATCH,
    JS_HTTP_DELETE,
    JS_HTTP_ALL
} js_http_method_t;

/* ---- struct ---- */

typedef struct {
    char *name;
    char *value;
} js_header_t;

typedef struct {
    js_http_method_t  method;
    char             *path;
    char             *query;
    js_header_t      *headers;
    int               header_count;
    char             *body;
    size_t            body_len;
    int               content_length;  /* -1 if absent */
} js_http_request_t;

typedef struct {
    int           status;
    js_header_t  *headers;
    int           header_count;
    char         *body;
    size_t        body_len;
} js_http_response_t;

/* ---- conn init ---- */

void js_http_conn_init(js_conn_t *conn);

/* ---- api ---- */

int              js_http_parse_request(js_buf_t *buf, js_http_request_t *req);
int              js_http_serialize_response(js_http_response_t *resp, js_buf_t *out);
const char      *js_http_status_text(int code);
js_http_method_t js_http_method_from_str(const char *str, int len);
void             js_http_request_free(js_http_request_t *req);
void             js_http_response_free(js_http_response_t *resp);

#endif
