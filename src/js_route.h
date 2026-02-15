#ifndef JS_ROUTE_H
#define JS_ROUTE_H

/* ---- struct ---- */

typedef struct {
    char *name;
    char *value;
} js_param_t;

typedef struct {
    char *str;       /* literal text or param name (without ':') */
    int   is_param;  /* 0 = literal, 1 = :param */
} js_segment_t;

typedef struct js_route_s {
    js_http_method_t    method;
    char               *pattern;       /* original: "/users/:id" */
    js_segment_t       *segments;
    int                 segment_count;
    JSValue             handler;       /* JS function, valid in current context only */
    struct js_route_s  *next;
} js_route_t;

typedef struct {
    js_route_t *route;
    js_param_t *params;
    int         param_count;
} js_route_match_t;

/* ---- api ---- */

js_route_t *js_route_add(js_route_t **head, js_http_method_t method,
                         const char *pattern, JSValue handler);
int         js_route_match(js_route_t *head, js_http_method_t method,
                           const char *path, js_route_match_t *result);
void        js_route_match_free(js_route_match_t *match);
void        js_route_free_all(js_route_t *head, JSContext *ctx);

#endif
