#include "js_main.h"
#include <fcntl.h>
#include <errno.h>

int js_runtime_init(js_runtime_t *rt) {
    memset(rt, 0, sizeof(*rt));
    rt->lfd = -1;
    if (js_store_init(&rt->store, 64) < 0)
        return -1;
    return 0;
}

int js_runtime_listen(js_runtime_t *rt) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(rt->port),
    };
    if (rt->host)
        inet_pton(AF_INET, rt->host, &addr.sin_addr);
    else
        addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 512) < 0) {
        close(fd);
        return -1;
    }

    rt->lfd = fd;
    return 0;
}

void js_runtime_free(js_runtime_t *rt) {
    /* free threads */
    for (int i = 0; i < rt->thread_count; i++)
        free(rt->threads[i]);
    free(rt->threads);

    /* close listen fd */
    if (rt->lfd >= 0)
        close(rt->lfd);

    /* free store */
    js_store_free(&rt->store);

    /* free bytecode */
    free(rt->bytecode);
    free(rt->host);

    memset(rt, 0, sizeof(*rt));
}
