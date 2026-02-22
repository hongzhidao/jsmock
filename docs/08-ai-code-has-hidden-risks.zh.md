# AI 写的代码有隐患

AI 写的代码有隐患。应对隐患最好的方式是人为 review + 测试用例。这在没有 AI 的时代也是最有效的方式。

这篇用一个具体的例子说明——一个 use-after-free，藏在第六篇 AI 写的 setTimeout 实现里，两篇文章、70 个测试都没发现。人为 review 发现了它。

## 隐患在哪

如果用户在 JS 文件顶层写 setTimeout：

```js
setTimeout(() => {}, 100);

mock.get("/api", (req) => {
    return new Response("ok");
});
```

这是合法的 JS。顶层代码在模块求值时执行，setTimeout 会在引擎的红黑树里注册一个定时器。

但看请求处理的同步退出路径——比如 404：

```c
if (!js_route_match(...)) {
    resp->status = 404;
    resp->body = strdup("Not Found");
    js_route_free_all(exec.routes, qctx);
    JS_FreeContext(qctx);
    JS_FreeRuntime(qrt);
    return 0;
}
```

Runtime 释放了。但定时器还挂在红黑树上，指向已释放的 context。定时器到期，`js_timeout_handler` 访问已释放的内存——use-after-free，段错误。

不只是 404。所有同步退出路径都有这个问题——fulfilled promise、sync handler、eval 失败——四五条路径，每一条都直接 free runtime，没有检查是否有残留的 timer。

## 为什么 70 个测试没发现

AI 写的测试和它的实现有相同的认知范围。

AI 实现 setTimeout 时，脑子里的用例是 handler 里用 setTimeout 延迟返回响应。它写的测试也全是这个场景：

```js
mock.get("/delayed", async (req) => {
    return new Promise((resolve) => {
        setTimeout(() => {
            resolve(new Response("delayed-ok"));
        }, 100);
    });
});
```

handler 内部用 setTimeout，走异步路径，runtime 不会提前释放。测试全过。

它不会想到"如果用户在顶层写 setTimeout 呢"——因为设计时就没考虑这个场景。测试自然也不会覆盖。

**代码和测试有一致的盲区，互相验证，制造了"没问题"的假象。**

这跟第六篇提到的 fetch 是同一个模式：AI 写的代码不检查错误，AI 写的测试不验证错误，16576 次请求"零错误"，实际每一次都失败了。

这就是为什么光靠 AI 自己写测试不够。测试用例需要人来补——补的不是数量，是 AI 想不到的角度。

## Review 发现问题

这个 bug 是 review 代码时发现的。看 `js_qjs_handle_request` 的退出路径，问一个问题：**如果顶层注册了 timer，这些 free 安全吗？** 答案是不安全。

这就是 review 的价值。它捕获的不是语法错误——编译器能抓。它捕获的是认知盲区：写代码的人没想到的边界条件、生命周期问题、资源释放遗漏。

一个人实现 setTimeout，脑子里想的是 handler 里用，测试也写 handler 里用。另一个人 review 时问一句"顶层调用呢"——这一个问题就能把隐患提前消灭。

## 修复

根因清楚了：四五条退出路径各自释放资源，没有统一检查 timer。

把所有退出路径归并到三个标签：

```
fail:      设置 500 响应，fall through ↓
done:      如果有残留 timer → goto deferred
           否则 → 同步返回
deferred:  堆上分配 exec，异步返回
```

所有路径先写 `exec.resp`，`done` 统一决定同步还是异步：

```c
done:
    if (exec.timeouts != NULL)
        goto deferred;

    *resp = exec.resp;
    memset(&exec.resp, 0, sizeof(exec.resp));
    js_route_free_all(exec.routes, qctx);
    JS_FreeContext(qctx);
    JS_FreeRuntime(qrt);
    return 0;

deferred:
    {
        js_exec_t *heap_exec = malloc(sizeof(*heap_exec));
        *heap_exec = exec;
        heap_exec->conn = conn;
        JS_SetContextOpaque(qctx, heap_exec);
        return 1;
    }
```

有残留 timer 就不释放 runtime，走异步路径。timer 到期后，现有的 `js_timeout_handler` 会检查 `exec->resolved && exec->timeouts == NULL`，调 `js_pending_finish` 发送响应并释放资源。这段代码不需要改——AI 当初写的异步收尾逻辑本身没问题，缺的只是让它有机会被执行到。

70 个原有测试全过，新增 4 个测试覆盖顶层 setTimeout 场景。

代码变更: [767cc09](https://github.com/hongzhidao/jsmock/commit/767cc09)

## 方法没变，角色变了

AI 写的代码有隐患，人写的代码也有隐患。隐患不分人和 AI。

没有 AI 的时代，应对隐患最有效的方式就是 review + 测试用例。有了 AI，方法没变，角色变了：以前同事之间互相 review，不同人的盲区不一样，互相补。现在 AI 写代码，它的代码和测试盲区高度一致，补位得你来。

AI 的实现能力很强——第六篇的 setTimeout，从 Promise 桥接到 C 层回调，从连接挂起到定时器恢复，架构清晰。但它不会主动做防御性思考："这个资源的生命周期在所有路径上都安全吗？"

这不需要新方法论来应对。Review + 测试用例，软件工程里最古老也最有效的手段，足够了。

这篇比较短，因为道理本身不复杂。但这个问题值得认真对待。AI 写代码的速度越来越快，一天能产出过去一周的量。产出快，隐患也跟着埋得快。如果 review 和测试跟不上，隐患就会在代码库里累积，直到某天在生产环境里爆出来。

速度是 AI 给的。质量还是得人把关。

---

专栏：[我带 AI 写了个项目](https://www.zhihu.com/column/c_2006330352843657698)

GitHub: https://github.com/hongzhidao/jsmock
