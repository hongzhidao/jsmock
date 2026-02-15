#ifndef JS_TIMER_H
#define JS_TIMER_H

/* ---- struct ---- */

typedef struct js_timer_entry_s {
    int                       fd;        /* connection fd */
    time_t                    deadline;
    struct js_timer_entry_s  *next;
} js_timer_entry_t;

typedef struct {
    int                tfd;       /* timerfd */
    js_timer_entry_t  *entries;   /* sorted by deadline */
} js_timer_t;

/* ---- api ---- */

int  js_timer_init(js_timer_t *tm);
int  js_timer_add(js_timer_t *tm, int fd, int timeout_sec);
int  js_timer_remove(js_timer_t *tm, int fd);
int  js_timer_sweep(js_timer_t *tm);  /* close expired, return count */
void js_timer_free(js_timer_t *tm);

#endif
