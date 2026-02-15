# 先啃最难的，其他的自然变简单

72 分不是终点。

上一篇，先设计架构再让 AI 写，第一版拿到 72 分，省掉了 jsbench 那 30 多次重构。但 jsbench 重构后到了 85 分——先设计不等于一步到位，后面还得迭代。

这篇记录第一次迭代。迭代要有顺序，我的原则是先啃最难的。

以前做项目，不少人习惯从简单的入手——先搭脚手架、先写工具函数、先把容易的模块做完。这在以前有道理：简单的做完能看到进度，积累信心。但现在简单的事 AI 几乎能代劳，路由匹配、HTTP 解析、KV 存储，上一篇的评分表里两个版本得分一样。真正拉开差距的是架构决策——并发模型、连接抽象、模块分层。既然简单的不再是瓶颈，注意力就该集中在 AI 做不好的地方。而那些地方往往也是最难的——不是算法难，是需要权衡、没有标准答案、选错了牵一发动全身。

## engine 的问题：盒子没封好

翻了一遍代码，从 js_engine.c 开始看。engine 是事件驱动架构的核心，也是最容易出架构问题的地方。设计阶段定的方向没问题，但落地的代码有明显的缺陷。

看 `js_engine_handle_conn` 这个函数：

```c
static void js_engine_handle_conn(js_engine_t *eng, js_conn_t *conn,
                                  uint32_t events) {
    if (events & EPOLLIN) {
        int rc = js_conn_read(conn);
        ...
        js_http_request_t req = {0};
        int parsed = js_http_parse_request(&conn->rbuf, &req);
        ...
        js_qjs_handle_request(rt, &req, &resp);
        ...
        js_http_serialize_response(&resp, &conn->wbuf);
        ...
    }
    if (events & EPOLLOUT) {
        js_conn_write(conn);
        ...
    }
}
```

engine 是事件层，却直接调了连接层的 `js_conn_read`、HTTP 层的 `js_http_parse_request`、应用层的 `js_qjs_handle_request`。第一篇里自己定的规则——依赖只能往下，事件层不碰应用层——engine 自己就违反了。

好的架构把复杂度装进盒子里，每个盒子对外只露简单的接口。engine 这个盒子没封好，业务逻辑漏了进来。nginx 的事件引擎不知道 HTTP 的存在，它只负责把事件分发给注册好的回调。而 jsmock 的 engine 同时在做"事件分发"和"请求处理"两件事。

问题看到了，但没法一步全改。engine 要和业务解耦，需要引入回调机制，连接管理、HTTP 处理都要跟着调整。一步全改容易出新问题，得找一个切入点。

## 从定时器开始

我选了定时器。

定时器是 engine 依赖链的底层——engine 组合 epoll 和 timer，timer 的设计如果有问题，engine 怎么改都会受限。而原来的 timer 设计确实有问题。

原来的定时器用 Linux 的 timerfd——创建一个文件描述符，注册到 epoll，到期后当 I/O 事件通知。timer entry 用排序链表管理。设计能用，但有几个问题：

timerfd 注册在 epoll 里，就变成了和 listen fd、连接 fd 并列的事件源，engine 的事件分发要多一路 `else if` 来区分"这个事件是 timer 的"。定时器和 I/O 事件本质不是一类东西，不该在同一层处理。链表插入 O(n)，每个 entry 单独 malloc。过期处理只是 free 掉 entry，代码里还留着一行 `/* TODO: caller should close the expired fd */`——写的时候没想清楚过期后该干什么。

## 新的定时器

新的定时器用红黑树管理超时，不再依赖 timerfd。和 engine 的交互只剩两个调用：`js_timer_find` 返回最近的超时毫秒数，传给 `epoll_wait` 做 timeout；`js_timer_expire` 在 epoll 返回后处理到期的 timer。

timer 从 epoll 里拿掉了，不再是一个 fd、一种事件源，而是 engine 可以使用的工具，事件分发从三路回到两路。链表换成红黑树，插入删除从 O(n) 变成 O(log n)，timer 节点通过 `container_of` 找回宿主结构体，不需要单独 malloc——nginx 里的经典做法。每个 timer 自带回调，旧设计里 timer entry 只存一个 fd，过期了不知道该干什么，新设计里 timer 带 handler 和 data，过期直接调回调，复杂度收在 timer 这个盒子里。

做了更难的事，调用方反而更简单了。难的部分做对了，周围的代码会跟着变简单。这也是我倾向先做难的原因。

代码变更: [7e40f2a](https://github.com/hongzhidao/jsmock/commit/7e40f2a)

## 一句话换来的设计

接下来改 engine 的事件分发。我跟 AI 说了一句：让事件处理更通用。

它引入了一个 `js_event_t` 结构体——每个事件自带 read 和 write 回调，engine 的事件循环只管拿到事件、调回调，不需要知道对面是 listen fd 还是连接。tag pointer 删了，`handle_conn` 拆成了独立的 `conn_read` 和 `conn_write` 回调，连接结构体也嵌入了 `js_event_t`。

之前 engine 认识所有人——listen fd、timer、连接，各走不同的分支。现在它谁都不认识，只管分发。engine 这个盒子终于封好了。

改动涉及四个文件，AI 一次改完，测试全过。定时器那次，设计方向是我定的，AI 负责实现。这次我只给了方向，它自己完成了设计。但"让事件处理更通用"这句话本身就包含了判断——当前设计哪里不对、该往什么方向改。这个判断还是得人来做。

代码变更: [4267a54](https://github.com/hongzhidao/jsmock/commit/4267a54)

## 封装：把复杂度藏起来

事件回调有了，但 engine 的事件循环里还在直接操作 epoll 的细节——遍历 events 数组、判断 EPOLLIN/EPOLLOUT、处理 EINTR。这些是 epoll 自己的事，不该暴露给 engine。

把 `js_epoll_wait` 升级成 `js_epoll_poll`，把事件遍历、回调分发、EINTR 处理全收进去。engine 不再碰 `epoll_event`，不再碰 `errno`，只需要知道"调一下 poll，事件就处理完了"。

架构归根结底是管理复杂度，封装是最基本的手段。每一层把自己的复杂度藏好，上一层看到的就是一个简单的接口。epoll 藏好事件分发，timer 藏好超时管理，engine 只管把它们串起来。

代码变更: [6dcf1eb](https://github.com/hongzhidao/jsmock/commit/6dcf1eb)

## 消除不合理

封装做完，engine_run 里还有一处不合理：启动时设置 listen socket——创建 listen 事件、注册到 epoll。这是初始化逻辑，不是事件循环该管的事。

把 listen socket 的注册移到 `js_thread_entry` 里，engine_run 变成纯粹的事件循环——poll 事件、处理超时，不碰任何业务。engine 不知道外面注册了什么事件，也不关心，它只负责转。

架构改进就是不断消除不合理。每次回头看代码，总能发现"这个东西不该放在这里"。一次改一个，每次改完都比上一版干净一点。不用一步到位，但方向要对。

代码变更: [1640ab3](https://github.com/hongzhidao/jsmock/commit/1640ab3)

## 四次改动之后

四次改动，都围绕 js_engine.c。改之前的 engine_run：

```c
void js_engine_run(js_engine_t *eng) {
    js_runtime_t *rt = js_thread_current->rt;
    int lfd = rt->lfd;

    js_epoll_add(&eng->epoll, lfd, EPOLLIN | EPOLLEXCLUSIVE, JS_TAG_LISTEN);

    for (;;) {
        int n = js_epoll_wait(&eng->epoll, 1000);
        if (n < 0) { ... }

        for (int i = 0; i < n; i++) {
            struct epoll_event *ev = &eng->epoll.events[i];
            void *ptr = ev->data.ptr;

            if (ptr == JS_TAG_LISTEN) {
                js_engine_accept(eng, lfd);
            } else if (ptr == JS_TAG_TIMER) {
                js_timer_sweep(&eng->timer);
            } else {
                js_conn_t *conn = ptr;
                js_engine_handle_conn(eng, conn, ev->events);
            }
        }
    }
}
```

改之后：

```c
void js_engine_run(js_engine_t *eng) {
    js_msec_t timeout;

    for (;;) {
        timeout = js_timer_find(&eng->timers);

        if (js_epoll_poll(&eng->epoll, (int) timeout) < 0)
            break;

        js_timer_expire(&eng->timers, js_engine_time());
    }
}
```

listen 注册、事件分发、EINTR 处理、tag pointer——全部不在了。engine 只做一件事：循环。算超时、poll 事件、处理到期。每一步一行调用，每个调用背后的复杂度都封在自己的盒子里。

几个体会：

**先啃最难的，不是因为难的有趣，是因为难的决定了其他东西的形状。** timer 的设计决定了 engine 的事件模型，engine 的回调机制决定了连接怎么接入。先改简单的，最后改到难的时候，前面的可能要返工。

**一次只改一个问题。** 四次改动分别解决四个问题：timer 耦合 epoll、事件分发硬编码、epoll 细节泄漏、listen 注册放错位置。一次全改容易出错，逐个击破反而更快到位。

**将军赶路，不追小兔。** 改的过程中有些命名、有些写法不算理想，但没必要当下纠正。架构对了，细节可以慢慢磨。反过来，架构不对的话，细节磨得再好也白费——下次重构照样推翻。

---

专栏：[我带 AI 写了个项目](https://www.zhihu.com/column/c_2006330352843657698)

GitHub: https://github.com/hongzhidao/jsmock
