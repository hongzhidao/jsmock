#include "js_main.h"

static js_msec_t js_engine_time(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (js_msec_t) (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int js_engine_init(js_engine_t *eng, int max_events) {
    if (js_epoll_init(&eng->epoll, max_events) < 0)
        return -1;
    js_timers_init(&eng->timers);
    return 0;
}

void js_engine_run(js_engine_t *eng) {
    js_msec_t timeout;

    for (;;) {
        timeout = js_timer_find(&eng->timers);

        if (js_epoll_poll(&eng->epoll, (int) timeout) < 0)
            break;

        js_timer_expire(&eng->timers, js_engine_time());
    }
}

void js_engine_free(js_engine_t *eng) {
    js_epoll_free(&eng->epoll);
}
