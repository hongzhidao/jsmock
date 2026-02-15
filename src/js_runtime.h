#ifndef JS_RUNTIME_H
#define JS_RUNTIME_H

/* ---- struct ---- */

typedef struct js_runtime_s {
    uint8_t       *bytecode;
    size_t         bytecode_len;
    char          *script_path;    /* original script path for re-compilation */
    char          *host;
    int            port;
    int            lfd;            /* listen fd */
    js_store_t     store;
    js_thread_t  **threads;
    int            thread_count;
} js_runtime_t;

/* ---- api ---- */

int  js_runtime_init(js_runtime_t *rt);
int  js_runtime_listen(js_runtime_t *rt);
void js_runtime_free(js_runtime_t *rt);

#endif
