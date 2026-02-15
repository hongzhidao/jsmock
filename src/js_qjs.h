#ifndef JS_QJS_H
#define JS_QJS_H

/* forward declarations */
struct js_runtime_s;

/* ---- struct ---- */

typedef struct {
    struct js_runtime_s *rt;       /* back pointer to global runtime */
    js_route_t          *routes;   /* per-request route list */
    JSRuntime           *qrt;      /* QuickJS runtime */
    JSContext           *qctx;     /* QuickJS context */
} js_exec_t;

/* ---- api ---- */

int  js_qjs_compile(const char *filename, uint8_t **out_buf, size_t *out_len);
int  js_qjs_read_config(const char *script_path, uint8_t *bytecode, size_t len,
                        char **host, int *port);
int  js_qjs_handle_request(struct js_runtime_s *rt,
                           js_http_request_t *req, js_http_response_t *resp);

#endif
