#include "js_main.h"
#include <sys/timerfd.h>

int js_timer_init(js_timer_t *tm) {
    tm->tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (tm->tfd < 0)
        return -1;
    tm->entries = NULL;
    return 0;
}

/* arm timerfd to fire at nearest deadline */
static void js_timer_arm(js_timer_t *tm) {
    struct itimerspec its = {0};
    if (tm->entries) {
        its.it_value.tv_sec = tm->entries->deadline;
    }
    /* all-zero its disarms the timer if no entries */
    timerfd_settime(tm->tfd, TFD_TIMER_ABSTIME, &its, NULL);
}

int js_timer_add(js_timer_t *tm, int fd, int timeout_sec) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    js_timer_entry_t *entry = malloc(sizeof(*entry));
    if (!entry)
        return -1;
    entry->fd = fd;
    entry->deadline = now.tv_sec + timeout_sec;

    /* insert sorted by deadline (ascending) */
    js_timer_entry_t **pp = &tm->entries;
    while (*pp && (*pp)->deadline <= entry->deadline)
        pp = &(*pp)->next;
    entry->next = *pp;
    *pp = entry;

    js_timer_arm(tm);
    return 0;
}

int js_timer_remove(js_timer_t *tm, int fd) {
    js_timer_entry_t **pp = &tm->entries;
    while (*pp) {
        if ((*pp)->fd == fd) {
            js_timer_entry_t *e = *pp;
            *pp = e->next;
            free(e);
            js_timer_arm(tm);
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1;
}

int js_timer_sweep(js_timer_t *tm) {
    /* consume timerfd */
    uint64_t exp;
    if (read(tm->tfd, &exp, sizeof(exp)) < 0)
        return 0;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    int count = 0;
    while (tm->entries && tm->entries->deadline <= now.tv_sec) {
        js_timer_entry_t *e = tm->entries;
        tm->entries = e->next;
        /* TODO: caller should close the expired fd */
        free(e);
        count++;
    }
    js_timer_arm(tm);
    return count;
}

void js_timer_free(js_timer_t *tm) {
    while (tm->entries) {
        js_timer_entry_t *e = tm->entries;
        tm->entries = e->next;
        free(e);
    }
    if (tm->tfd >= 0)
        close(tm->tfd);
    tm->tfd = -1;
}
