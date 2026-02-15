#include "js_main.h"

/* ---- class IDs ---- */

static JSClassID js_response_class_id;
static JSClassID js_request_class_id;
static JSClassID js_headers_class_id;
static JSClassID js_url_class_id;

/* ==== Headers class ==== */

static void js_headers_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    JSHeadersData *d = JS_GetOpaque(val, js_headers_class_id);
    if (d) {
        for (int i = 0; i < d->count; i++) {
            free(d->entries[i].name);
            free(d->entries[i].value);
        }
        free(d->entries);
        free(d);
    }
}

static JSClassDef js_headers_class = {
    "Headers",
    .finalizer = js_headers_finalizer,
};

static JSValue js_headers_ctor(JSContext *ctx, JSValueConst new_target,
                               int argc, JSValue *argv) {
    (void)new_target;
    JSHeadersData *d = calloc(1, sizeof(*d));
    JSValue obj = JS_NewObjectClass(ctx, js_headers_class_id);
    JS_SetOpaque(obj, d);

    /* optional init object */
    if (argc >= 1 && JS_IsObject(argv[0])) {
        JSPropertyEnum *props;
        uint32_t count;
        if (JS_GetOwnPropertyNames(ctx, &props, &count, argv[0],
                                   JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
            d->entries = calloc(count, sizeof(js_header_t));
            d->cap = count;
            for (uint32_t i = 0; i < count; i++) {
                const char *key = JS_AtomToCString(ctx, props[i].atom);
                JSValue val = JS_GetProperty(ctx, argv[0], props[i].atom);
                const char *valstr = JS_ToCString(ctx, val);
                d->entries[d->count].name = strdup(key);
                d->entries[d->count].value = strdup(valstr);
                d->count++;
                JS_FreeCString(ctx, valstr);
                JS_FreeValue(ctx, val);
                JS_FreeCString(ctx, key);
                JS_FreeAtom(ctx, props[i].atom);
            }
            js_free(ctx, props);
        }
    }
    return obj;
}

static JSValue js_headers_get(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValue *argv) {
    (void)argc;
    JSHeadersData *d = JS_GetOpaque2(ctx, this_val, js_headers_class_id);
    if (!d) return JS_EXCEPTION;
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;
    for (int i = 0; i < d->count; i++) {
        if (strcasecmp(d->entries[i].name, name) == 0) {
            JS_FreeCString(ctx, name);
            return JS_NewString(ctx, d->entries[i].value);
        }
    }
    JS_FreeCString(ctx, name);
    return JS_NULL;
}

static JSValue js_headers_set(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValue *argv) {
    (void)argc;
    JSHeadersData *d = JS_GetOpaque2(ctx, this_val, js_headers_class_id);
    if (!d) return JS_EXCEPTION;
    const char *name = JS_ToCString(ctx, argv[0]);
    const char *value = JS_ToCString(ctx, argv[1]);
    if (!name || !value) {
        JS_FreeCString(ctx, name);
        JS_FreeCString(ctx, value);
        return JS_EXCEPTION;
    }
    /* update existing */
    for (int i = 0; i < d->count; i++) {
        if (strcasecmp(d->entries[i].name, name) == 0) {
            free(d->entries[i].value);
            d->entries[i].value = strdup(value);
            JS_FreeCString(ctx, name);
            JS_FreeCString(ctx, value);
            return JS_UNDEFINED;
        }
    }
    /* append */
    if (d->count >= d->cap) {
        d->cap = d->cap ? d->cap * 2 : 8;
        d->entries = realloc(d->entries, d->cap * sizeof(js_header_t));
    }
    d->entries[d->count].name = strdup(name);
    d->entries[d->count].value = strdup(value);
    d->count++;
    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, value);
    return JS_UNDEFINED;
}

static JSValue js_headers_has(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValue *argv) {
    (void)argc;
    JSHeadersData *d = JS_GetOpaque2(ctx, this_val, js_headers_class_id);
    if (!d) return JS_EXCEPTION;
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;
    for (int i = 0; i < d->count; i++) {
        if (strcasecmp(d->entries[i].name, name) == 0) {
            JS_FreeCString(ctx, name);
            return JS_TRUE;
        }
    }
    JS_FreeCString(ctx, name);
    return JS_FALSE;
}

static JSValue js_headers_delete(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValue *argv) {
    (void)argc;
    JSHeadersData *d = JS_GetOpaque2(ctx, this_val, js_headers_class_id);
    if (!d) return JS_EXCEPTION;
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;
    for (int i = 0; i < d->count; i++) {
        if (strcasecmp(d->entries[i].name, name) == 0) {
            free(d->entries[i].name);
            free(d->entries[i].value);
            memmove(&d->entries[i], &d->entries[i + 1],
                    (d->count - i - 1) * sizeof(js_header_t));
            d->count--;
            JS_FreeCString(ctx, name);
            return JS_UNDEFINED;
        }
    }
    JS_FreeCString(ctx, name);
    return JS_UNDEFINED;
}

/* helper: create a JS Headers object from C header array */
static JSValue js_headers_from_c(JSContext *ctx, js_header_t *headers,
                                 int count) {
    JSValue obj = JS_NewObjectClass(ctx, js_headers_class_id);
    JSHeadersData *d = calloc(1, sizeof(*d));
    d->entries = calloc(count, sizeof(js_header_t));
    d->count = count;
    d->cap = count;
    for (int i = 0; i < count; i++) {
        d->entries[i].name = strdup(headers[i].name);
        d->entries[i].value = strdup(headers[i].value);
    }
    JS_SetOpaque(obj, d);
    return obj;
}

/* ==== URL class ==== */

static void js_url_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    JSURLData *d = JS_GetOpaque(val, js_url_class_id);
    if (d) {
        free(d->href);
        free(d->protocol);
        free(d->host);
        free(d->hostname);
        free(d->port);
        free(d->pathname);
        free(d->search);
        free(d->hash);
        free(d);
    }
}

static JSClassDef js_url_class = {
    "URL",
    .finalizer = js_url_finalizer,
};

/* simple URL parser: "http://host:port/path?query#hash" */
static JSURLData *js_url_parse(const char *str) {
    JSURLData *d = calloc(1, sizeof(*d));
    d->href = strdup(str);

    const char *p = str;

    /* protocol */
    const char *colon = strstr(p, "://");
    if (colon) {
        d->protocol = strndup(p, colon - p + 1); /* "http:" */
        p = colon + 3;
    } else {
        d->protocol = strdup("");
        /* relative URL, treat whole thing as pathname */
        const char *q = strchr(p, '?');
        const char *h = strchr(p, '#');
        if (q) {
            d->pathname = strndup(p, q - p);
            if (h && h > q)
                d->search = strndup(q, h - q);
            else
                d->search = strdup(q);
            if (h)
                d->hash = strdup(h);
        } else if (h) {
            d->pathname = strndup(p, h - p);
            d->hash = strdup(h);
        } else {
            d->pathname = strdup(p);
        }
        if (!d->hostname) d->hostname = strdup("");
        if (!d->host) d->host = strdup("");
        if (!d->port) d->port = strdup("");
        if (!d->pathname) d->pathname = strdup("");
        if (!d->search) d->search = strdup("");
        if (!d->hash) d->hash = strdup("");
        return d;
    }

    /* host:port */
    const char *slash = strchr(p, '/');
    size_t hostlen = slash ? (size_t)(slash - p) : strlen(p);
    char *hostport = strndup(p, hostlen);

    char *port_sep = strrchr(hostport, ':');
    if (port_sep) {
        d->hostname = strndup(hostport, port_sep - hostport);
        d->port = strdup(port_sep + 1);
        d->host = strdup(hostport);
    } else {
        d->hostname = strdup(hostport);
        d->port = strdup("");
        d->host = strdup(hostport);
    }
    free(hostport);

    /* path?query#hash */
    if (slash) {
        p = slash;
        const char *q = strchr(p, '?');
        const char *h = strchr(p, '#');
        if (q) {
            d->pathname = strndup(p, q - p);
            if (h && h > q)
                d->search = strndup(q, h - q);
            else
                d->search = strdup(q);
        } else if (h) {
            d->pathname = strndup(p, h - p);
        } else {
            d->pathname = strdup(p);
        }
        if (h)
            d->hash = strdup(h);
    }

    if (!d->pathname) d->pathname = strdup("/");
    if (!d->search) d->search = strdup("");
    if (!d->hash) d->hash = strdup("");

    return d;
}

static JSValue js_url_ctor(JSContext *ctx, JSValueConst new_target,
                           int argc, JSValue *argv) {
    (void)new_target; (void)argc;
    const char *str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_EXCEPTION;
    JSURLData *d = js_url_parse(str);
    JS_FreeCString(ctx, str);

    JSValue obj = JS_NewObjectClass(ctx, js_url_class_id);
    JS_SetOpaque(obj, d);
    return obj;
}

static JSValue js_url_get_prop(JSContext *ctx, JSValueConst this_val,
                               int magic) {
    JSURLData *d = JS_GetOpaque2(ctx, this_val, js_url_class_id);
    if (!d) return JS_EXCEPTION;
    switch (magic) {
    case 0: return JS_NewString(ctx, d->href);
    case 1: return JS_NewString(ctx, d->protocol);
    case 2: return JS_NewString(ctx, d->host);
    case 3: return JS_NewString(ctx, d->hostname);
    case 4: return JS_NewString(ctx, d->port);
    case 5: return JS_NewString(ctx, d->pathname);
    case 6: return JS_NewString(ctx, d->search);
    case 7: return JS_NewString(ctx, d->hash);
    default: return JS_UNDEFINED;
    }
}

/* URLSearchParams.get(name) — returns first value or null */
static JSValue js_search_params_get(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValue *argv) {
    (void)argc;
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;
    JSValue entries = JS_GetPropertyStr(ctx, this_val, "_entries");
    int32_t len;
    JS_ToInt32(ctx, &len, JS_GetPropertyStr(ctx, entries, "length"));
    for (int32_t i = 0; i < len; i++) {
        JSValue entry = JS_GetPropertyUint32(ctx, entries, i);
        JSValue k = JS_GetPropertyUint32(ctx, entry, 0);
        const char *key = JS_ToCString(ctx, k);
        if (key && strcmp(key, name) == 0) {
            JSValue v = JS_GetPropertyUint32(ctx, entry, 1);
            JS_FreeCString(ctx, key);
            JS_FreeValue(ctx, k);
            JS_FreeValue(ctx, entry);
            JS_FreeValue(ctx, entries);
            JS_FreeCString(ctx, name);
            return v;
        }
        JS_FreeCString(ctx, key);
        JS_FreeValue(ctx, k);
        JS_FreeValue(ctx, entry);
    }
    JS_FreeValue(ctx, entries);
    JS_FreeCString(ctx, name);
    return JS_NULL;
}

/* searchParams getter — returns object with get() method and _entries array */
static JSValue js_url_get_search_params(JSContext *ctx, JSValueConst this_val,
                                        int magic) {
    (void)magic;
    JSURLData *d = JS_GetOpaque2(ctx, this_val, js_url_class_id);
    if (!d) return JS_EXCEPTION;

    JSValue obj = JS_NewObject(ctx);
    JSValue entries = JS_NewArray(ctx);
    int idx = 0;

    const char *s = d->search;
    if (s && *s == '?') s++;

    while (s && *s) {
        const char *amp = strchr(s, '&');
        size_t pairlen = amp ? (size_t)(amp - s) : strlen(s);
        char *pair = strndup(s, pairlen);

        char *eq = strchr(pair, '=');
        JSValue entry = JS_NewArray(ctx);
        if (eq) {
            *eq = '\0';
            JS_SetPropertyUint32(ctx, entry, 0, JS_NewString(ctx, pair));
            JS_SetPropertyUint32(ctx, entry, 1, JS_NewString(ctx, eq + 1));
        } else {
            JS_SetPropertyUint32(ctx, entry, 0, JS_NewString(ctx, pair));
            JS_SetPropertyUint32(ctx, entry, 1, JS_NewString(ctx, ""));
        }
        JS_SetPropertyUint32(ctx, entries, idx++, entry);
        free(pair);
        s = amp ? amp + 1 : NULL;
    }

    JS_SetPropertyStr(ctx, obj, "_entries", entries);
    JS_SetPropertyStr(ctx, obj, "get",
                      JS_NewCFunction(ctx, js_search_params_get, "get", 1));
    return obj;
}

/* ==== Request class ==== */

static void js_request_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    JSRequestData *d = JS_GetOpaque(val, js_request_class_id);
    if (d) {
        free(d->method);
        free(d->url);
        for (int i = 0; i < d->header_count; i++) {
            free(d->headers[i].name);
            free(d->headers[i].value);
        }
        free(d->headers);
        free(d->body);
        for (int i = 0; i < d->param_count; i++) {
            free(d->params[i].name);
            free(d->params[i].value);
        }
        free(d->params);
        free(d);
    }
}

static JSClassDef js_request_class = {
    "Request",
    .finalizer = js_request_finalizer,
};

static const char *js_http_method_str(js_http_method_t m) {
    switch (m) {
    case JS_HTTP_GET:    return "GET";
    case JS_HTTP_POST:   return "POST";
    case JS_HTTP_PUT:    return "PUT";
    case JS_HTTP_PATCH:  return "PATCH";
    case JS_HTTP_DELETE: return "DELETE";
    default:             return "GET";
    }
}

/* build JS Request object from C request + route match params */
JSValue js_web_new_request(JSContext *ctx, js_http_request_t *req,
                           js_param_t *params, int param_count) {
    JSRequestData *d = calloc(1, sizeof(*d));
    d->method = strdup(js_http_method_str(req->method));
    d->url = strdup(req->path);

    /* copy headers */
    if (req->header_count > 0) {
        d->headers = calloc(req->header_count, sizeof(js_header_t));
        d->header_count = req->header_count;
        for (int i = 0; i < req->header_count; i++) {
            d->headers[i].name = strdup(req->headers[i].name);
            d->headers[i].value = strdup(req->headers[i].value);
        }
    }

    /* copy body */
    if (req->body && req->body_len > 0) {
        d->body = strndup(req->body, req->body_len);
        d->body_len = req->body_len;
    }

    /* copy params */
    if (param_count > 0) {
        d->params = calloc(param_count, sizeof(js_param_t));
        d->param_count = param_count;
        for (int i = 0; i < param_count; i++) {
            d->params[i].name = strdup(params[i].name);
            d->params[i].value = strdup(params[i].value);
        }
    }

    JSValue obj = JS_NewObjectClass(ctx, js_request_class_id);
    JS_SetOpaque(obj, d);
    return obj;
}

static JSValue js_request_get_method(JSContext *ctx, JSValueConst this_val,
                                     int magic) {
    (void)magic;
    JSRequestData *d = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!d) return JS_EXCEPTION;
    return JS_NewString(ctx, d->method);
}

static JSValue js_request_get_url(JSContext *ctx, JSValueConst this_val,
                                  int magic) {
    (void)magic;
    JSRequestData *d = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!d) return JS_EXCEPTION;
    return JS_NewString(ctx, d->url);
}

static JSValue js_request_get_headers(JSContext *ctx, JSValueConst this_val,
                                      int magic) {
    (void)magic;
    JSRequestData *d = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!d) return JS_EXCEPTION;
    return js_headers_from_c(ctx, d->headers, d->header_count);
}

static JSValue js_request_get_params(JSContext *ctx, JSValueConst this_val,
                                     int magic) {
    (void)magic;
    JSRequestData *d = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!d) return JS_EXCEPTION;
    JSValue obj = JS_NewObject(ctx);
    for (int i = 0; i < d->param_count; i++) {
        JS_SetPropertyStr(ctx, obj, d->params[i].name,
                          JS_NewString(ctx, d->params[i].value));
    }
    return obj;
}

static JSValue js_request_text(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValue *argv) {
    (void)argc; (void)argv;
    JSRequestData *d = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!d) return JS_EXCEPTION;
    if (d->body)
        return JS_NewStringLen(ctx, d->body, d->body_len);
    return JS_NewString(ctx, "");
}

static JSValue js_request_json(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValue *argv) {
    (void)argc; (void)argv;
    JSRequestData *d = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!d) return JS_EXCEPTION;
    if (!d->body)
        return JS_NULL;
    return JS_ParseJSON(ctx, d->body, d->body_len, "<request>");
}

/* ==== Response class ==== */

static void js_response_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    JSResponseData *d = JS_GetOpaque(val, js_response_class_id);
    if (d) {
        for (int i = 0; i < d->header_count; i++) {
            free(d->headers[i].name);
            free(d->headers[i].value);
        }
        free(d->headers);
        free(d->body);
        free(d);
    }
}

static JSClassDef js_response_class = {
    "Response",
    .finalizer = js_response_finalizer,
};

static JSValue js_response_ctor(JSContext *ctx, JSValueConst new_target,
                                int argc, JSValue *argv) {
    (void)new_target;
    JSResponseData *d = calloc(1, sizeof(*d));
    d->status = 200;

    /* arg0: body (string or null) */
    if (argc >= 1 && !JS_IsNull(argv[0]) && !JS_IsUndefined(argv[0])) {
        size_t len;
        const char *str = JS_ToCStringLen(ctx, &len, argv[0]);
        if (str) {
            d->body = strndup(str, len);
            d->body_len = len;
            JS_FreeCString(ctx, str);
        }
    }

    /* arg1: options { status, headers } */
    if (argc >= 2 && JS_IsObject(argv[1])) {
        JSValue status_val = JS_GetPropertyStr(ctx, argv[1], "status");
        if (JS_IsNumber(status_val)) {
            int32_t s;
            JS_ToInt32(ctx, &s, status_val);
            d->status = s;
        }
        JS_FreeValue(ctx, status_val);

        JSValue hdrs = JS_GetPropertyStr(ctx, argv[1], "headers");
        if (JS_IsObject(hdrs)) {
            JSPropertyEnum *props;
            uint32_t prop_count;
            if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, hdrs,
                                       JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
                d->headers = calloc(prop_count, sizeof(js_header_t));
                d->header_count = prop_count;
                for (uint32_t i = 0; i < prop_count; i++) {
                    const char *key = JS_AtomToCString(ctx, props[i].atom);
                    JSValue val = JS_GetProperty(ctx, hdrs, props[i].atom);
                    const char *valstr = JS_ToCString(ctx, val);
                    d->headers[i].name = strdup(key);
                    d->headers[i].value = strdup(valstr);
                    JS_FreeCString(ctx, valstr);
                    JS_FreeValue(ctx, val);
                    JS_FreeCString(ctx, key);
                    JS_FreeAtom(ctx, props[i].atom);
                }
                js_free(ctx, props);
            }
        }
        JS_FreeValue(ctx, hdrs);
    }

    JSValue obj = JS_NewObjectClass(ctx, js_response_class_id);
    JS_SetOpaque(obj, d);
    return obj;
}

/* ==== TextEncoder / TextDecoder ==== */

static JSValue js_textencoder_ctor(JSContext *ctx, JSValueConst new_target,
                                   int argc, JSValue *argv) {
    (void)ctx; (void)argc; (void)argv;
    return JS_GetPropertyStr(ctx, new_target, "prototype");
}

static JSValue js_textdecoder_ctor(JSContext *ctx, JSValueConst new_target,
                                   int argc, JSValue *argv) {
    (void)ctx; (void)argc; (void)argv;
    return JS_GetPropertyStr(ctx, new_target, "prototype");
}

static JSValue js_textencoder_encode(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValue *argv) {
    (void)this_val; (void)argc;
    size_t len;
    const char *str = JS_ToCStringLen(ctx, &len, argv[0]);
    if (!str) return JS_EXCEPTION;
    JSValue buf = JS_NewArrayBufferCopy(ctx, (const uint8_t *)str, len);
    JS_FreeCString(ctx, str);
    /* wrap in Uint8Array */
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue uint8_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
    JS_FreeValue(ctx, global);
    JSValue result = JS_CallConstructor(ctx, uint8_ctor, 1, &buf);
    JS_FreeValue(ctx, uint8_ctor);
    JS_FreeValue(ctx, buf);
    return result;
}

static JSValue js_textdecoder_decode(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValue *argv) {
    (void)this_val; (void)argc;
    size_t len;
    uint8_t *buf = JS_GetArrayBuffer(ctx, &len, argv[0]);
    if (buf)
        return JS_NewStringLen(ctx, (const char *)buf, len);

    /* try typed array */
    size_t offset, blen;
    JSValue ab = JS_GetTypedArrayBuffer(ctx, argv[0], &offset, &blen, NULL);
    if (!JS_IsException(ab)) {
        buf = JS_GetArrayBuffer(ctx, &len, ab);
        JSValue result = JS_UNDEFINED;
        if (buf)
            result = JS_NewStringLen(ctx, (const char *)(buf + offset), blen);
        JS_FreeValue(ctx, ab);
        return result;
    }
    return JS_NewString(ctx, "");
}

/* ==== mock.get/post/put/patch/delete/all bindings ==== */

static js_exec_t *js_web_get_exec(JSContext *ctx) {
    return JS_GetContextOpaque(ctx);
}

static JSValue js_mock_route(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValue *argv,
                             js_http_method_t method) {
    (void)this_val;
    if (argc < 2) return JS_UNDEFINED;
    const char *pattern = JS_ToCString(ctx, argv[0]);
    if (!pattern) return JS_EXCEPTION;
    js_exec_t *exec = js_web_get_exec(ctx);
    js_route_add(&exec->routes, method, pattern, JS_DupValue(ctx, argv[1]));
    JS_FreeCString(ctx, pattern);
    return JS_UNDEFINED;
}

static JSValue js_mock_get(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValue *argv) {
    return js_mock_route(ctx, this_val, argc, argv, JS_HTTP_GET);
}
static JSValue js_mock_post(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValue *argv) {
    return js_mock_route(ctx, this_val, argc, argv, JS_HTTP_POST);
}
static JSValue js_mock_put(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValue *argv) {
    return js_mock_route(ctx, this_val, argc, argv, JS_HTTP_PUT);
}
static JSValue js_mock_patch(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValue *argv) {
    return js_mock_route(ctx, this_val, argc, argv, JS_HTTP_PATCH);
}
static JSValue js_mock_delete(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValue *argv) {
    return js_mock_route(ctx, this_val, argc, argv, JS_HTTP_DELETE);
}
static JSValue js_mock_all(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValue *argv) {
    return js_mock_route(ctx, this_val, argc, argv, JS_HTTP_ALL);
}

/* ==== mock.env ==== */

static JSValue js_mock_env(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValue *argv) {
    (void)this_val; (void)argc;
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;
    const char *val = getenv(name);
    JS_FreeCString(ctx, name);
    return val ? JS_NewString(ctx, val) : JS_UNDEFINED;
}

/* ==== mock.store bindings ==== */

static JSValue js_store_js_get(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValue *argv) {
    (void)this_val; (void)argc;
    js_exec_t *exec = js_web_get_exec(ctx);
    const char *key = JS_ToCString(ctx, argv[0]);
    if (!key) return JS_EXCEPTION;
    char *val = js_store_get(&exec->rt->store, key);
    JS_FreeCString(ctx, key);
    if (!val) return JS_UNDEFINED;
    /* parse stored JSON value */
    JSValue result = JS_ParseJSON(ctx, val, strlen(val), "<store>");
    free(val);
    return result;
}

static JSValue js_store_js_set(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValue *argv) {
    (void)this_val; (void)argc;
    js_exec_t *exec = js_web_get_exec(ctx);
    const char *key = JS_ToCString(ctx, argv[0]);
    if (!key) return JS_EXCEPTION;
    /* serialize value to JSON */
    JSValue json = JS_JSONStringify(ctx, argv[1], JS_UNDEFINED, JS_UNDEFINED);
    const char *val = JS_ToCString(ctx, json);
    JS_FreeValue(ctx, json);
    if (!val) { JS_FreeCString(ctx, key); return JS_EXCEPTION; }
    js_store_set(&exec->rt->store, key, val);
    JS_FreeCString(ctx, key);
    JS_FreeCString(ctx, val);
    return JS_UNDEFINED;
}

static JSValue js_store_js_del(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValue *argv) {
    (void)this_val; (void)argc;
    js_exec_t *exec = js_web_get_exec(ctx);
    const char *key = JS_ToCString(ctx, argv[0]);
    if (!key) return JS_EXCEPTION;
    js_store_del(&exec->rt->store, key);
    JS_FreeCString(ctx, key);
    return JS_UNDEFINED;
}

static JSValue js_store_js_incr(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValue *argv) {
    (void)this_val; (void)argc;
    js_exec_t *exec = js_web_get_exec(ctx);
    const char *key = JS_ToCString(ctx, argv[0]);
    if (!key) return JS_EXCEPTION;
    int val = js_store_incr(&exec->rt->store, key);
    JS_FreeCString(ctx, key);
    return JS_NewInt32(ctx, val);
}

static JSValue js_store_js_clear(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValue *argv) {
    (void)this_val; (void)argc; (void)argv;
    js_exec_t *exec = js_web_get_exec(ctx);
    js_store_clear(&exec->rt->store);
    return JS_UNDEFINED;
}

/* ==== console.log ==== */

static JSValue js_console_log(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValue *argv) {
    (void)this_val;
    for (int i = 0; i < argc; i++) {
        const char *str = JS_ToCString(ctx, argv[i]);
        if (str) {
            if (i > 0) fputc(' ', stderr);
            fputs(str, stderr);
            JS_FreeCString(ctx, str);
        }
    }
    fputc('\n', stderr);
    return JS_UNDEFINED;
}

/* ==== init: register all Web APIs ==== */

void js_web_init(js_exec_t *exec) {
    JSContext *ctx = exec->qctx;
    JSRuntime *rt = exec->qrt;
    JS_SetContextOpaque(ctx, exec);

    JSValue global = JS_GetGlobalObject(ctx);

    /* ---- Headers class ---- */
    JS_NewClassID(&js_headers_class_id);
    JS_NewClass(rt, js_headers_class_id, &js_headers_class);
    JSValue hdrs_proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, hdrs_proto, "get",
                      JS_NewCFunction(ctx, js_headers_get, "get", 1));
    JS_SetPropertyStr(ctx, hdrs_proto, "set",
                      JS_NewCFunction(ctx, js_headers_set, "set", 2));
    JS_SetPropertyStr(ctx, hdrs_proto, "has",
                      JS_NewCFunction(ctx, js_headers_has, "has", 1));
    JS_SetPropertyStr(ctx, hdrs_proto, "delete",
                      JS_NewCFunction(ctx, js_headers_delete, "delete", 1));
    JSValue hdrs_ctor = JS_NewCFunction2(ctx, js_headers_ctor, "Headers", 1,
                                        JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, hdrs_ctor, hdrs_proto);
    JS_SetClassProto(ctx, js_headers_class_id, hdrs_proto);
    JS_SetPropertyStr(ctx, global, "Headers", hdrs_ctor);

    /* ---- URL class ---- */
    JS_NewClassID(&js_url_class_id);
    JS_NewClass(rt, js_url_class_id, &js_url_class);
    JSValue url_proto = JS_NewObject(ctx);
    static const JSCFunctionListEntry url_getters[] = {
        JS_CGETSET_MAGIC_DEF("href", js_url_get_prop, NULL, 0),
        JS_CGETSET_MAGIC_DEF("protocol", js_url_get_prop, NULL, 1),
        JS_CGETSET_MAGIC_DEF("host", js_url_get_prop, NULL, 2),
        JS_CGETSET_MAGIC_DEF("hostname", js_url_get_prop, NULL, 3),
        JS_CGETSET_MAGIC_DEF("port", js_url_get_prop, NULL, 4),
        JS_CGETSET_MAGIC_DEF("pathname", js_url_get_prop, NULL, 5),
        JS_CGETSET_MAGIC_DEF("search", js_url_get_prop, NULL, 6),
        JS_CGETSET_MAGIC_DEF("hash", js_url_get_prop, NULL, 7),
        JS_CGETSET_MAGIC_DEF("searchParams", js_url_get_search_params, NULL, 0),
    };
    JS_SetPropertyFunctionList(ctx, url_proto, url_getters,
                               sizeof(url_getters) / sizeof(url_getters[0]));
    JSValue url_ctor = JS_NewCFunction2(ctx, js_url_ctor, "URL", 1,
                                       JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, url_ctor, url_proto);
    JS_SetClassProto(ctx, js_url_class_id, url_proto);
    JS_SetPropertyStr(ctx, global, "URL", url_ctor);

    /* ---- Request class ---- */
    JS_NewClassID(&js_request_class_id);
    JS_NewClass(rt, js_request_class_id, &js_request_class);
    JSValue req_proto = JS_NewObject(ctx);
    static const JSCFunctionListEntry req_getters[] = {
        JS_CGETSET_MAGIC_DEF("method", js_request_get_method, NULL, 0),
        JS_CGETSET_MAGIC_DEF("url", js_request_get_url, NULL, 0),
        JS_CGETSET_MAGIC_DEF("headers", js_request_get_headers, NULL, 0),
        JS_CGETSET_MAGIC_DEF("params", js_request_get_params, NULL, 0),
    };
    JS_SetPropertyFunctionList(ctx, req_proto, req_getters,
                               sizeof(req_getters) / sizeof(req_getters[0]));
    JS_SetPropertyStr(ctx, req_proto, "text",
                      JS_NewCFunction(ctx, js_request_text, "text", 0));
    JS_SetPropertyStr(ctx, req_proto, "json",
                      JS_NewCFunction(ctx, js_request_json, "json", 0));
    JS_SetClassProto(ctx, js_request_class_id, req_proto);

    /* ---- Response class ---- */
    JS_NewClassID(&js_response_class_id);
    JS_NewClass(rt, js_response_class_id, &js_response_class);
    JSValue resp_proto = JS_NewObject(ctx);
    JSValue resp_ctor = JS_NewCFunction2(ctx, js_response_ctor, "Response", 2,
                                        JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, resp_ctor, resp_proto);
    JS_SetClassProto(ctx, js_response_class_id, resp_proto);
    JS_SetPropertyStr(ctx, global, "Response", resp_ctor);

    /* ---- TextEncoder: new TextEncoder().encode(str) ---- */
    JSValue te_proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, te_proto, "encode",
                      JS_NewCFunction(ctx, js_textencoder_encode, "encode", 1));
    JSValue te_ctor = JS_NewCFunction2(ctx, js_textencoder_ctor, "TextEncoder", 0,
                                       JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, te_ctor, te_proto);
    JS_SetPropertyStr(ctx, global, "TextEncoder", te_ctor);
    JS_FreeValue(ctx, te_proto);

    /* ---- TextDecoder: new TextDecoder().decode(bytes) ---- */
    JSValue td_proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, td_proto, "decode",
                      JS_NewCFunction(ctx, js_textdecoder_decode, "decode", 1));
    JSValue td_ctor = JS_NewCFunction2(ctx, js_textdecoder_ctor, "TextDecoder", 0,
                                       JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, td_ctor, td_proto);
    JS_SetPropertyStr(ctx, global, "TextDecoder", td_ctor);
    JS_FreeValue(ctx, td_proto);

    /* ---- console ---- */
    JSValue console = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console, "log",
                      JS_NewCFunction(ctx, js_console_log, "log", 1));
    JS_SetPropertyStr(ctx, global, "console", console);

    /* ---- mock object ---- */
    JSValue mock = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, mock, "get", JS_NewCFunction(ctx, js_mock_get, "get", 2));
    JS_SetPropertyStr(ctx, mock, "post", JS_NewCFunction(ctx, js_mock_post, "post", 2));
    JS_SetPropertyStr(ctx, mock, "put", JS_NewCFunction(ctx, js_mock_put, "put", 2));
    JS_SetPropertyStr(ctx, mock, "patch", JS_NewCFunction(ctx, js_mock_patch, "patch", 2));
    JS_SetPropertyStr(ctx, mock, "delete", JS_NewCFunction(ctx, js_mock_delete, "delete", 2));
    JS_SetPropertyStr(ctx, mock, "all", JS_NewCFunction(ctx, js_mock_all, "all", 2));
    JS_SetPropertyStr(ctx, mock, "env", JS_NewCFunction(ctx, js_mock_env, "env", 1));

    /* ---- mock.store ---- */
    JSValue store = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, store, "get", JS_NewCFunction(ctx, js_store_js_get, "get", 1));
    JS_SetPropertyStr(ctx, store, "set", JS_NewCFunction(ctx, js_store_js_set, "set", 2));
    JS_SetPropertyStr(ctx, store, "del", JS_NewCFunction(ctx, js_store_js_del, "del", 1));
    JS_SetPropertyStr(ctx, store, "incr", JS_NewCFunction(ctx, js_store_js_incr, "incr", 1));
    JS_SetPropertyStr(ctx, store, "clear", JS_NewCFunction(ctx, js_store_js_clear, "clear", 0));
    JS_SetPropertyStr(ctx, mock, "store", store);

    JS_SetPropertyStr(ctx, global, "mock", mock);
    JS_FreeValue(ctx, global);
}

int js_web_read_response(JSContext *ctx, JSValue val, js_http_response_t *resp) {
    JSResponseData *d = JS_GetOpaque2(ctx, val, js_response_class_id);
    if (!d)
        return -1;

    resp->status = d->status;
    resp->body = d->body ? strdup(d->body) : NULL;
    resp->body_len = d->body_len;

    if (d->header_count > 0) {
        resp->headers = calloc(d->header_count, sizeof(js_header_t));
        resp->header_count = d->header_count;
        for (int i = 0; i < d->header_count; i++) {
            resp->headers[i].name = strdup(d->headers[i].name);
            resp->headers[i].value = strdup(d->headers[i].value);
        }
    }
    return 0;
}
