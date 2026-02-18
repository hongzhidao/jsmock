# 代码没有 bug，但放错了地方

上一篇改完四次，engine_run 只剩 8 行。打开 js_engine.c，文件却有 100 多行。

多出来的是三个函数：`js_engine_accept`、`js_engine_conn_read`、`js_engine_conn_write`。accept 创建连接，conn_read 读数据、解析 HTTP、执行 JS 脚本、写响应，conn_write 发数据、发完关连接。

这些代码没有 bug，逻辑清楚，测试全过。但 engine 是事件层，这三个函数调用了连接层的 `js_conn_read`、HTTP 层的 `js_http_parse_request`、应用层的 `js_qjs_handle_request`。事件引擎认识系统里的每一层。

上一篇讲封装——把复杂度藏进盒子。但封装没回答一个问题：这段代码该放在哪个盒子里？

## 封装和职责不是一回事

封装解决"怎么藏"。上一篇把 epoll 的事件遍历收进 `js_epoll_poll`，timer 的超时计算收进 `js_timer_find`。调用方不看里面的细节，只看接口。

职责解决"该归谁"。engine 里这三个函数，封装得挺好——每个函数职责单一，输入输出清楚。但它们不该出现在这个文件里。

accept 的本质是创建连接。conn_read 驱动连接的读流程。conn_write 驱动连接的写流程。它们操作的对象是连接，驱动的是连接从建立到关闭的完整生命周期。这是 conn 模块的事，不是 engine 的事。

为什么会放错？因为 AI 写代码的时候正在写 engine 的事件循环，事件来了要处理，顺手就把处理逻辑放在了同一个文件里。代码是按"写的顺序"组织的，不是按"该在的位置"组织的。

## 搬家

三个函数从 js_engine.c 移到 js_conn.c。`js_engine_accept` 改名 `js_listen_accept`，`js_engine_conn_read` 改名 `js_conn_on_read`，`js_engine_conn_write` 改名 `js_conn_on_write`。

搬过去之后有个有意思的变化：两个事件回调变成了 static。在 engine 里它们必须是 public 的，因为 accept 要被 thread 引用。搬到 conn 之后，accept 通过一个新的 `js_listen_start` API 暴露，内部的回调可以藏起来了。职责放对了，封装自然跟着变好。

js_thread.c 也跟着简化了，之前要手动设置 listen 事件——填 fd、填回调、填 epoll flag，五行代码，thread 需要知道 listen 事件的所有内部细节。现在一行 `js_listen_start`，细节收在 conn 模块里。

## 35 行

搬完之后的 js_engine.c：

```c
#include "js_main.h"

static js_msec_t js_engine_time(void) { ... }

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
```

35 行。init、run、free，加一个内部的时间函数。不 include `<fcntl.h>`，不认识连接，不认识 HTTP，不认识 JS 执行。engine 的职责是事件循环，现在它只做事件循环。

js_conn.c 的结构变成了：buf 操作 → listen/accept → 连接创建/读写/关闭 → 事件回调。从 buffer 到监听到生命周期到事件处理，连接相关的代码终于住在一起了。

## 放错位置的代价不是 bug

这次改动，行为没变，测试全过，功能完全一样。AI 不会主动做这种事。它判断代码质量的标准是"能不能正确运行"，不是"该不该放在这里"。

但对维护者来说，打开 js_engine.c 看到 HTTP 解析和 JS 执行，第一反应是困惑——事件引擎跟这些有什么关系？代码放错位置不会导致 bug，但会增加理解的成本。每个读代码的人都要在脑子里做一次重新归类。

更实际的问题是改动范围。如果以后连接的读写逻辑要改——加 keep-alive、改成流式响应——改的是 conn 的逻辑，却要去动 engine 的文件。职责错位让改动扩散到不该扩散的地方。

搬完之后这个问题消失了。连接的事改 js_conn.c，引擎的事改 js_engine.c，各管各的。

代码变更: [709eae0](https://github.com/hongzhidao/jsmock/commit/709eae0)

## 人也会放错

搬完代码，回头看 js_main.h 里的 include 顺序，发现自己也有放得不对的地方。

当初设计模块布局的时候，我把 js_thread.h 放在紧跟 js_engine.h 后面。直觉上觉得线程和事件循环关系最近——线程启动事件循环，事件循环跑在线程里，放一起很自然。

但 jsmock 的分层是：engine → conn → http → runtime/thread。engine 是最底层的事件基础设施，conn 处理连接，http 处理协议，thread 和 runtime 在最上面做编排——创建 engine、启动循环、连接配置。thread 是管理者，不是事件层的一部分。它该在 runtime 旁边，不该在 engine 旁边。

前半篇说 AI 按写代码的顺序放东西，没想过该放在哪。这件事上我自己也带着旧项目的惯性——经验是双刃剑，帮你做判断，也会让你把旧项目的结构套到新项目上。

AI 和人不是谁对谁错的零和博弈。我发现了 AI 放错的代码，AI 的分层也让我重新审视了自己的判断。而且有 AI 在，试错的成本很低——觉得放得不对，说一句，几十处引用它帮你改完。以前调整模块布局要小心翼翼，现在可以大胆尝试，不合适再调回来。人负责判断该怎么放，AI 负责快速落地，不对就再来一轮。互相补着走，比谁都不敢动要好。

代码变更: [34cd8f4](https://github.com/hongzhidao/jsmock/commit/34cd8f4)

## buf 不是 conn 的事

回头看 js_conn.c，开头还有一段 buf 操作——init、append、consume、free。上一篇搬 handlers 的时候没动它，因为 conn 用 buf 做读写缓冲，放在一起看着也顺眼。

但 buf 是通用的数据结构。conn 用它做读写缓冲，http 用它序列化响应，以后加 WebSocket 或者中间件也会用到。它不属于任何一个模块，它是基础设施。放在 conn 里只是因为 conn 最先用到它——又是按写的顺序放的。

提取成独立的 js_buf.h 和 js_buf.c，在 js_main.h 里放在 conn 前面。conn 依赖 buf，buf 不知道 conn 的存在。

这个改动比前两个更简单，但道理是一样的：通用的东西不该绑在具体的模块上。绑了也不会出 bug，但下一个用 buf 的模块就得 include js_conn.h——为了用一个缓冲区，得引入连接的所有定义。依赖关系就是这样一步步变乱的。

代码变更: [f09af8c](https://github.com/hongzhidao/jsmock/commit/f09af8c)

## 将来谁来维护

这三次改动，动机都是让人更容易理解代码。handlers 放在 engine 里人会困惑，thread 放在 engine 旁边人会误解依赖关系，buf 绑在 conn 里人会引入不必要的依赖。

但如果将来维护代码的主要是 AI 呢？AI 不会因为打开 js_engine.c 看到 HTTP 解析而困惑，它能瞬间扫完所有文件搞清楚调用关系。它不需要靠文件名和 include 顺序来理解分层。按这个逻辑，代码放在哪似乎没那么重要了。

我觉得没这么简单。职责划分不只是为了读代码的人，更是为了控制改动的范围。handlers 放在 engine 里，改连接逻辑就要动 engine 文件；buf 绑在 conn 里，新模块用 buf 就要依赖 conn。不管是人改还是 AI 改，职责不清都会让改动扩散到不该扩散的地方。AI 改得快不代表改得对——扩散的改动意味着更多的出错机会。

但架构的侧重点确实会变。现在我花很多精力在"让代码读起来清楚"上——命名、分文件、include 顺序。如果 AI 越来越多地参与维护，这些面向人的可读性可能没那么关键了，而依赖方向、模块边界、接口契约这些面向机器也能理解的硬约束会变得更重要。

谁来维护软件，决定了架构该为谁设计。这个问题现在还没有答案，但值得开始想了。

## 三个例子，一个概念

这篇只用了三个例子来体现"职责"这个概念。jsmock 里肯定还有更多放错位置的代码——每次回头看都能发现新的。职责划分不是一次做完的事，是随着对系统理解加深不断调整的过程。

几个判断代码是否放对了位置的方法：看它操作的对象属于谁——handlers 操作连接，就该在 conn 里。看它被谁依赖——buf 被多个模块用，就不该绑在某一个模块上。看改动的时候要不要动不相关的文件——如果改连接逻辑要动 engine，说明有东西放错了。

上一篇讲的封装是把复杂度藏起来，这篇讲的职责是把代码放对地方。两个都是管理复杂度的手段，但解决的问题不同。封装做好了，调用方不需要知道实现细节。职责做好了，改一个模块不会牵连到其他模块。先封装再调职责，顺序不能反——东西没装好就搬家，搬完还是一团乱。

---

专栏：[我带 AI 写了个项目](https://www.zhihu.com/column/c_2006330352843657698)

GitHub: https://github.com/hongzhidao/jsmock
