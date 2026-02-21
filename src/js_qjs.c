#include "js_main.h"

/* ---- file reading helper ---- */

static char *js_qjs_read_file(const char *path, size_t *plen) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    if (fread(buf, 1, len, f) != (size_t)len) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[len] = '\0';
    fclose(f);
    *plen = len;
    return buf;
}

/* ---- module loader for ES6 imports ---- */

static char *js_module_normalize(JSContext *ctx, const char *base_name,
                                 const char *name, void *opaque) {
    (void)opaque;
    if (name[0] != '.' && name[0] != '/') {
        return js_strdup(ctx, name);
    }
    const char *slash = strrchr(base_name, '/');
    size_t dirlen = slash ? (size_t)(slash - base_name + 1) : 0;
    const char *rel = name;
    if (rel[0] == '.' && rel[1] == '/') rel += 2;
    size_t namelen = strlen(rel);
    char *resolved = js_malloc(ctx, dirlen + namelen + 1);
    if (!resolved) return NULL;
    if (dirlen > 0)
        memcpy(resolved, base_name, dirlen);
    memcpy(resolved + dirlen, rel, namelen);
    resolved[dirlen + namelen] = '\0';
    return resolved;
}

static JSModuleDef *js_module_loader(JSContext *ctx, const char *module_name,
                                     void *opaque) {
    (void)opaque;
    size_t buf_len;
    char *buf = js_qjs_read_file(module_name, &buf_len);
    if (!buf) {
        JS_ThrowReferenceError(ctx, "could not load module '%s'", module_name);
        return NULL;
    }
    JSValue func_val = JS_Eval(ctx, buf, buf_len, module_name,
                               JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    free(buf);
    if (JS_IsException(func_val))
        return NULL;
    JSModuleDef *m = JS_VALUE_GET_PTR(func_val);
    JS_FreeValue(ctx, func_val);
    return m;
}

/* ---- stub globals for config reading ---- */

static JSValue js_stub_noop(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

static void js_qjs_register_stubs(JSContext *ctx) {
    JSValue global = JS_GetGlobalObject(ctx);

    JSValue mock = JS_NewObject(ctx);
    const char *methods[] = {"get","post","put","patch","delete","all","env",NULL};
    for (int i = 0; methods[i]; i++)
        JS_SetPropertyStr(ctx, mock, methods[i],
                          JS_NewCFunction(ctx, js_stub_noop, methods[i], 2));

    JSValue store = JS_NewObject(ctx);
    const char *smethods[] = {"get","set","del","incr","clear",NULL};
    for (int i = 0; smethods[i]; i++)
        JS_SetPropertyStr(ctx, store, smethods[i],
                          JS_NewCFunction(ctx, js_stub_noop, smethods[i], 2));
    JS_SetPropertyStr(ctx, mock, "store", store);
    JS_SetPropertyStr(ctx, global, "mock", mock);

    JS_SetPropertyStr(ctx, global, "Response",
                      JS_NewCFunction(ctx, js_stub_noop, "Response", 2));
    JS_SetPropertyStr(ctx, global, "Headers",
                      JS_NewCFunction(ctx, js_stub_noop, "Headers", 0));
    JS_SetPropertyStr(ctx, global, "URL",
                      JS_NewCFunction(ctx, js_stub_noop, "URL", 1));
    JS_SetPropertyStr(ctx, global, "TextEncoder",
                      JS_NewCFunction(ctx, js_stub_noop, "TextEncoder", 0));
    JS_SetPropertyStr(ctx, global, "TextDecoder",
                      JS_NewCFunction(ctx, js_stub_noop, "TextDecoder", 0));

    JSValue console = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console, "log",
                      JS_NewCFunction(ctx, js_stub_noop, "log", 1));
    JS_SetPropertyStr(ctx, global, "console", console);

    JS_SetPropertyStr(ctx, global, "setTimeout",
                      JS_NewCFunction(ctx, js_stub_noop, "setTimeout", 2));

    JS_FreeValue(ctx, global);
}

/* ---- public API ---- */

int js_qjs_compile(const char *filename, uint8_t **out_buf, size_t *out_len) {
    size_t fsize;
    char *src = js_qjs_read_file(filename, &fsize);
    if (!src) return -1;

    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JS_SetModuleLoaderFunc(rt, js_module_normalize, js_module_loader, NULL);

    int flags = JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY;
    JSValue obj = JS_Eval(ctx, src, fsize, filename, flags);
    free(src);

    if (JS_IsException(obj)) {
        JS_FreeValue(ctx, obj);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        return -1;
    }

    *out_buf = JS_WriteObject(ctx, out_len, obj, JS_WRITE_OBJ_BYTECODE);
    JS_FreeValue(ctx, obj);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    return *out_buf ? 0 : -1;
}

int js_qjs_read_config(const char *script_path, uint8_t *bytecode, size_t len,
                       char **host, int *port) {
    (void)bytecode; (void)len;

    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JS_SetModuleLoaderFunc(rt, js_module_normalize, js_module_loader, NULL);
    js_qjs_register_stubs(ctx);

    /* compile and evaluate from source */
    size_t slen;
    char *src = js_qjs_read_file(script_path, &slen);
    if (!src) goto fail;

    JSValue obj = JS_Eval(ctx, src, slen, script_path,
                          JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    free(src);
    if (JS_IsException(obj)) goto fail;

    JSModuleDef *m = NULL;
    if (JS_VALUE_GET_TAG(obj) == JS_TAG_MODULE)
        m = JS_VALUE_GET_PTR(obj);

    JSValue result = JS_EvalFunction(ctx, obj);
    if (JS_IsException(result)) goto fail;
    JS_FreeValue(ctx, result);

    JSContext *ctx1;
    while (JS_ExecutePendingJob(rt, &ctx1) > 0)
        ;

    if (!m) goto fail;

    JSValue ns = JS_GetModuleNamespace(ctx, m);
    if (JS_IsException(ns)) goto fail;

    JSValue def = JS_GetPropertyStr(ctx, ns, "default");
    JS_FreeValue(ctx, ns);
    if (JS_IsUndefined(def) || JS_IsException(def)) goto fail;

    JSValue listen_val = JS_GetPropertyStr(ctx, def, "listen");
    JS_FreeValue(ctx, def);

    if (JS_IsNumber(listen_val)) {
        int32_t p;
        JS_ToInt32(ctx, &p, listen_val);
        *port = p;
        *host = NULL;
    } else if (JS_IsString(listen_val)) {
        const char *str = JS_ToCString(ctx, listen_val);
        const char *colon = strrchr(str, ':');
        if (colon) {
            *host = strndup(str, colon - str);
            *port = atoi(colon + 1);
        } else {
            *port = atoi(str);
            *host = NULL;
        }
        JS_FreeCString(ctx, str);
    }
    JS_FreeValue(ctx, listen_val);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;

fail:
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return -1;
}

/* ---- Promise resolve/reject callbacks ---- */

static JSValue js_promise_on_resolve(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv) {
    (void)this_val;
    js_exec_t *exec = JS_GetContextOpaque(ctx);

    if (argc >= 1)
        js_web_read_response(ctx, argv[0], &exec->resp);
    else {
        exec->resp.status = 500;
        exec->resp.body = strdup("Internal Server Error");
        exec->resp.body_len = 21;
    }
    exec->resolved = 1;
    return JS_UNDEFINED;
}

static JSValue js_promise_on_reject(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    js_exec_t *exec = JS_GetContextOpaque(ctx);
    exec->resp.status = 500;
    exec->resp.body = strdup("Internal Server Error");
    exec->resp.body_len = 21;
    exec->resolved = 1;
    return JS_UNDEFINED;
}

/* ---- async lifecycle ---- */

void js_pending_finish(js_exec_t *exec) {
    js_engine_t *eng = &js_thread_current->engine;
    js_conn_t *conn = exec->conn;

    /* cancel any outstanding timers */
    js_timeout_t *to = exec->timeouts;
    while (to) {
        js_timeout_t *next = to->next;
        js_timer_delete(&eng->timers, &to->timer);
        JS_FreeValue(exec->qctx, to->cb);
        free(to);
        to = next;
    }
    exec->timeouts = NULL;

    /* serialize response into conn write buffer */
    js_http_serialize_response(&exec->resp, &conn->wbuf, conn->keep_alive);
    js_http_response_free(&exec->resp);

    /* resume the connection for writing */
    conn->state = JS_CONN_WRITING;
    js_epoll_add(&eng->epoll, conn->event.fd, EPOLLOUT, &conn->event);

    /* cleanup JS state */
    js_route_free_all(exec->routes, exec->qctx);
    JS_FreeContext(exec->qctx);
    JS_FreeRuntime(exec->qrt);
    free(exec);
}

int js_qjs_handle_request(js_runtime_t *rt,
                          js_http_request_t *req, js_http_response_t *resp,
                          js_conn_t *conn) {
    JSRuntime *qrt = JS_NewRuntime();
    JSContext *qctx = JS_NewContext(qrt);
    JS_SetModuleLoaderFunc(qrt, js_module_normalize, js_module_loader, NULL);

    js_exec_t exec = {
        .rt = rt,
        .routes = NULL,
        .qrt = qrt,
        .qctx = qctx,
        .conn = NULL,
        .resp = {0},
        .resolved = 0,
        .timeouts = NULL,
    };

    /* register Web API bindings */
    js_web_init(&exec);

    /* load pre-compiled bytecode (compiled once at startup) */
    JSValue obj = JS_ReadObject(qctx, rt->bytecode, rt->bytecode_len,
                                JS_READ_OBJ_BYTECODE);
    if (JS_IsException(obj)) goto fail;

    /* resolve imported modules before evaluation */
    if (JS_ResolveModule(qctx, obj) < 0) {
        JS_FreeValue(qctx, obj);
        goto fail;
    }

    JSValue result = JS_EvalFunction(qctx, obj);
    if (JS_IsException(result)) {
        JS_FreeValue(qctx, result);
        goto fail;
    }
    JS_FreeValue(qctx, result);

    /* drain pending jobs (for async module evaluation) */
    JSContext *pctx;
    while (JS_ExecutePendingJob(qrt, &pctx) > 0)
        ;

    /* match route */
    js_route_match_t match = {0};
    if (!js_route_match(exec.routes, req->method, req->path, &match)) {
        resp->status = 404;
        resp->body = strdup("Not Found");
        resp->body_len = 9;
        js_route_free_all(exec.routes, qctx);
        JS_FreeContext(qctx);
        JS_FreeRuntime(qrt);
        return 0;
    }

    /* build JS Request object and call handler */
    JSValue js_req = js_web_new_request(qctx, req,
                                        match.params, match.param_count);
    JSValue handler_result = JS_Call(qctx, match.route->handler,
                                    JS_UNDEFINED, 1, &js_req);
    JS_FreeValue(qctx, js_req);
    js_route_match_free(&match);

    if (JS_IsException(handler_result)) {
        JS_FreeValue(qctx, handler_result);
        goto fail;
    }

    /* drain pending jobs after handler call */
    while (JS_ExecutePendingJob(qrt, &pctx) > 0)
        ;

    /* check if result is a Promise */
    JSPromiseStateEnum state = JS_PromiseState(qctx, handler_result);

    if (state == JS_PROMISE_FULFILLED) {
        /* already resolved — extract result synchronously */
        JSValue resolved_val = JS_PromiseResult(qctx, handler_result);
        JS_FreeValue(qctx, handler_result);
        js_web_read_response(qctx, resolved_val, resp);
        JS_FreeValue(qctx, resolved_val);

        js_route_free_all(exec.routes, qctx);
        JS_FreeContext(qctx);
        JS_FreeRuntime(qrt);
        return 0;

    } else if (state == JS_PROMISE_REJECTED) {
        JS_FreeValue(qctx, handler_result);
        goto fail;

    } else if (state == JS_PROMISE_PENDING) {
        /* attach .then(onResolve, onReject) */
        JSValue on_resolve = JS_NewCFunction(qctx, js_promise_on_resolve,
                                             "onResolve", 1);
        JSValue on_reject = JS_NewCFunction(qctx, js_promise_on_reject,
                                            "onReject", 1);
        JSValue args[2] = { on_resolve, on_reject };
        JSValue then_result = JS_Invoke(qctx, handler_result,
                                        JS_NewAtom(qctx, "then"), 2, args);
        JS_FreeValue(qctx, then_result);
        JS_FreeValue(qctx, on_resolve);
        JS_FreeValue(qctx, on_reject);
        JS_FreeValue(qctx, handler_result);

        /* drain again — may resolve immediately */
        while (JS_ExecutePendingJob(qrt, &pctx) > 0)
            ;

        if (exec.resolved && exec.timeouts == NULL) {
            /* resolved during drain (e.g., async handler with no real await) */
            *resp = exec.resp;
            memset(&exec.resp, 0, sizeof(exec.resp));

            js_route_free_all(exec.routes, qctx);
            JS_RunGC(qrt);
            JS_FreeContext(qctx);
            JS_FreeRuntime(qrt);
            return 0;
        }

        /* truly async — allocate exec on heap, return 1 */
        js_exec_t *heap_exec = malloc(sizeof(*heap_exec));
        *heap_exec = exec;
        heap_exec->conn = conn;
        JS_SetContextOpaque(qctx, heap_exec);
        return 1;

    } else {
        /* not a Promise — sync path (plain Response object) */
        js_web_read_response(qctx, handler_result, resp);
        JS_FreeValue(qctx, handler_result);

        js_route_free_all(exec.routes, qctx);
        JS_FreeContext(qctx);
        JS_FreeRuntime(qrt);
        return 0;
    }

fail:
    js_route_free_all(exec.routes, qctx);
    JS_FreeContext(qctx);
    JS_FreeRuntime(qrt);
    return -1;
}
