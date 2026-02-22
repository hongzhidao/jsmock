#ifndef JS_QJS_H
#define JS_QJS_H

/* forward declarations */
struct js_runtime_s;

/* ---- struct ---- */

typedef struct js_timeout_s js_timeout_t;

typedef struct {
    struct js_runtime_s *rt;       /* back pointer to global runtime */
    js_route_t          *routes;   /* per-request route list */
    JSRuntime           *qrt;      /* QuickJS runtime */
    JSContext           *qctx;     /* QuickJS context */
    /* async support */
    js_conn_t           *conn;     /* NULL for sync */
    js_http_response_t   resp;     /* filled by .then() callback */
    int                  resolved; /* 1 = .then() invoked */
    js_timeout_t        *timeouts; /* linked list of pending timers */
} js_exec_t;

struct js_timeout_s {
    js_timer_t       timer;   /* MUST be first for container_of */
    js_timeout_t    *next;    /* linked list in exec->timeouts */
    JSContext        *qctx;   /* exec retrieved via JS_GetContextOpaque */
    JSValue          cb;
};

/* ---- api ---- */

int  js_qjs_compile(const char *filename, uint8_t **out_buf, size_t *out_len);
int  js_qjs_read_config(const char *script_path, uint8_t *bytecode, size_t len,
                        char **host, int *port);
int  js_qjs_handle_request(struct js_runtime_s *rt,
                           js_http_request_t *req, js_http_response_t *resp,
                           js_conn_t *conn);
/* returns: 0=sync (response in *resp), 1=async (response sent later) */

void js_pending_finish(js_exec_t *exec);

#endif
