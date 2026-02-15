#include "js_main.h"

/* parse "/users/:id/posts" into segments */
static int js_route_parse(const char *pattern, js_segment_t **out, int *count) {
    *out = NULL;
    *count = 0;

    const char *p = pattern;
    if (*p == '/') p++;

    while (*p) {
        const char *slash = strchr(p, '/');
        size_t len = slash ? (size_t)(slash - p) : strlen(p);

        *out = realloc(*out, sizeof(js_segment_t) * (*count + 1));
        if (!*out) return -1;

        js_segment_t *seg = &(*out)[*count];
        if (*p == ':') {
            seg->str = strndup(p + 1, len - 1);
            seg->is_param = 1;
        } else {
            seg->str = strndup(p, len);
            seg->is_param = 0;
        }
        (*count)++;

        if (!slash) break;
        p = slash + 1;
    }
    return 0;
}

js_route_t *js_route_add(js_route_t **head, js_http_method_t method,
                         const char *pattern, JSValue handler) {
    js_route_t *r = calloc(1, sizeof(*r));
    if (!r) return NULL;

    r->method = method;
    r->pattern = strdup(pattern);
    r->handler = handler;

    if (js_route_parse(pattern, &r->segments, &r->segment_count) < 0) {
        free(r->pattern);
        free(r);
        return NULL;
    }

    /* append to end of list */
    js_route_t **pp = head;
    while (*pp) pp = &(*pp)->next;
    *pp = r;
    r->next = NULL;

    return r;
}

int js_route_match(js_route_t *head, js_http_method_t method,
                   const char *path, js_route_match_t *result) {
    /* split path into segments */
    js_segment_t *path_segs = NULL;
    int path_count = 0;
    js_route_parse(path, &path_segs, &path_count);

    for (js_route_t *r = head; r; r = r->next) {
        /* check method */
        if (r->method != JS_HTTP_ALL && r->method != method)
            continue;
        /* check segment count */
        if (r->segment_count != path_count)
            continue;

        /* match each segment */
        int matched = 1;
        int param_count = 0;
        js_param_t *params = NULL;

        for (int i = 0; i < r->segment_count; i++) {
            if (r->segments[i].is_param) {
                params = realloc(params, sizeof(js_param_t) * (param_count + 1));
                params[param_count].name = strdup(r->segments[i].str);
                params[param_count].value = strdup(path_segs[i].str);
                param_count++;
            } else if (strcmp(r->segments[i].str, path_segs[i].str) != 0) {
                matched = 0;
                break;
            }
        }

        if (matched) {
            result->route = r;
            result->params = params;
            result->param_count = param_count;
            /* free path segments */
            for (int i = 0; i < path_count; i++) free(path_segs[i].str);
            free(path_segs);
            return 1;
        }

        /* free params on mismatch */
        for (int i = 0; i < param_count; i++) {
            free(params[i].name);
            free(params[i].value);
        }
        free(params);
    }

    /* free path segments */
    for (int i = 0; i < path_count; i++) free(path_segs[i].str);
    free(path_segs);
    return 0;
}

void js_route_match_free(js_route_match_t *match) {
    for (int i = 0; i < match->param_count; i++) {
        free(match->params[i].name);
        free(match->params[i].value);
    }
    free(match->params);
    match->params = NULL;
    match->param_count = 0;
}

void js_route_free_all(js_route_t *head, JSContext *ctx) {
    while (head) {
        js_route_t *next = head->next;
        free(head->pattern);
        for (int i = 0; i < head->segment_count; i++)
            free(head->segments[i].str);
        free(head->segments);
        JS_FreeValue(ctx, head->handler);
        free(head);
        head = next;
    }
}
