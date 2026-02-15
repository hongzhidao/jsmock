#ifndef JS_WEB_H
#define JS_WEB_H

/* ---- JS binding layer structs ---- */

typedef struct {
    char        *method;
    char        *url;
    js_header_t *headers;
    int          header_count;
    char        *body;
    size_t       body_len;
    js_param_t  *params;
    int          param_count;
} JSRequestData;

typedef struct {
    int          status;
    js_header_t *headers;
    int          header_count;
    char        *body;
    size_t       body_len;
} JSResponseData;

typedef struct {
    js_header_t *entries;
    int          count;
    int          cap;
} JSHeadersData;

typedef struct {
    char *href;
    char *protocol;
    char *host;
    char *hostname;
    char *port;
    char *pathname;
    char *search;
    char *hash;
} JSURLData;

/* ---- api ---- */

void    js_web_init(js_exec_t *exec);
JSValue js_web_new_request(JSContext *ctx, js_http_request_t *req,
                           js_param_t *params, int param_count);
int     js_web_read_response(JSContext *ctx, JSValue val, js_http_response_t *resp);

#endif
