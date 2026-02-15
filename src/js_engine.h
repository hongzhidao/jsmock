#ifndef JS_ENGINE_H
#define JS_ENGINE_H

/* ---- struct ---- */

typedef struct {
    js_epoll_t   epoll;
    js_timers_t  timers;
} js_engine_t;

/* ---- api ---- */

int  js_engine_init(js_engine_t *eng, int max_events);
void js_engine_accept(js_event_t *ev);
void js_engine_run(js_engine_t *eng);   /* main event loop */
void js_engine_free(js_engine_t *eng);

#endif
