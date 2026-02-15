#include "js_main.h"

__thread js_thread_t *js_thread_current = NULL;

static void *js_thread_entry(void *arg) {
    js_thread_t *t = arg;
    js_thread_current = t;

    if (js_engine_init(&t->engine, 1024) < 0) {
        fprintf(stderr, "thread %d: engine init failed\n", t->id);
        return NULL;
    }

    js_engine_run(&t->engine);
    js_engine_free(&t->engine);
    return NULL;
}

int js_thread_spawn_all(js_runtime_t *rt, int count) {
    rt->thread_count = count;
    rt->threads = calloc(count, sizeof(js_thread_t *));
    if (!rt->threads)
        return -1;

    for (int i = 0; i < count; i++) {
        rt->threads[i] = calloc(1, sizeof(js_thread_t));
        if (!rt->threads[i])
            return -1;
        rt->threads[i]->id = i;
        rt->threads[i]->rt = rt;

        if (pthread_create(&rt->threads[i]->tid, NULL,
                           js_thread_entry, rt->threads[i]) != 0) {
            fprintf(stderr, "failed to create thread %d\n", i);
            return -1;
        }
    }
    return 0;
}

void js_thread_wait_all(js_runtime_t *rt) {
    for (int i = 0; i < rt->thread_count; i++) {
        if (rt->threads[i])
            pthread_join(rt->threads[i]->tid, NULL);
    }
}
