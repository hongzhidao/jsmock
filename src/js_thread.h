#ifndef JS_THREAD_H
#define JS_THREAD_H

/* forward declaration */
struct js_runtime_s;

/* ---- struct ---- */

typedef struct {
    pthread_t            tid;
    int                  id;        /* thread index */
    js_engine_t          engine;
    struct js_runtime_s *rt;        /* back pointer to global runtime */
} js_thread_t;

/* thread-local pointer to current thread */
extern __thread js_thread_t *js_thread_current;

/* ---- api ---- */

int  js_thread_spawn_all(struct js_runtime_s *rt, int count);
void js_thread_wait_all(struct js_runtime_s *rt);

#endif
