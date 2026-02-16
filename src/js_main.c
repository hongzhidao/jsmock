#include "js_main.h"

static void js_usage(const char *prog) {
    fprintf(stderr, "Usage: %s <script.js>\n", prog);
    exit(1);
}

int main(int argc, char **argv) {
    if (argc < 2)
        js_usage(argv[0]);

    const char *script = argv[1];

    /* 1. compile script to bytecode */
    uint8_t *bytecode = NULL;
    size_t bytecode_len = 0;
    if (js_qjs_compile(script, &bytecode, &bytecode_len) < 0) {
        fprintf(stderr, "error: failed to compile %s\n", script);
        return 1;
    }

    /* 2. read config: export default { listen } */
    char *host = NULL;
    int port = 3000; /* default port */
    js_qjs_read_config(script, bytecode, bytecode_len, &host, &port);

    /* 3. init runtime */
    js_runtime_t rt;
    if (js_runtime_init(&rt) < 0) {
        fprintf(stderr, "error: runtime init failed\n");
        return 1;
    }
    rt.bytecode = bytecode;
    rt.bytecode_len = bytecode_len;
    rt.script_path = strdup(script);
    rt.host = host;
    rt.port = port;

    /* 4. create listen socket */
    if (js_runtime_listen(&rt) < 0) {
        fprintf(stderr, "error: failed to listen on %s:%d (%s)\n",
                host ? host : "0.0.0.0", port, strerror(errno));
        js_runtime_free(&rt);
        return 1;
    }

    fprintf(stderr, "jsmock listening on %s:%d\n",
            host ? host : "0.0.0.0", port);

    /* 5. spawn worker threads */
    int nthreads = 4; /* TODO: make configurable */
    if (js_thread_spawn_all(&rt, nthreads) < 0) {
        fprintf(stderr, "error: failed to spawn threads\n");
        js_runtime_free(&rt);
        return 1;
    }

    /* 6. wait */
    js_thread_wait_all(&rt);

    /* 7. cleanup */
    js_runtime_free(&rt);
    return 0;
}
