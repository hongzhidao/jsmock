# 让 AI 自己做性能优化

第六篇结尾提到 Kent Beck 的那句话：Make it work, make it right, make it fast。前六篇一直在做前两步。这篇做第三步——性能优化。同样的方式：一句话，不引导，看 AI 能做到什么程度。

## 我先看到了问题

做这次实验之前，我自己看过一遍请求处理的代码。一个细节立刻跳出来了。

jsmock 启动时，`js_main.c` 里调 `js_qjs_compile()` 把用户脚本编译成字节码，存进 `rt->bytecode`。但处理请求时，`js_qjs_handle_request()` 完全无视这份字节码——每次都从磁盘重新读源文件，重新编译：

```c
char *src = js_qjs_read_file(rt->script_path, &src_len);
JSValue obj = JS_Eval(qctx, src, src_len, rt->script_path,
                      JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
```

每次请求都做一次文件 I/O，跑一遍词法分析、语法分析、字节码生成。而结果跟启动时编译的一模一样。

这个问题我没告诉 AI。我想看它自己能不能发现。

## 一句话

跟 AI 说：分析 JS 的运行，最大程度提升性能。

AI 做的第一件事让我满意——它完整地走了一遍每次请求的执行路径：

```
请求进入
  → fopen() 读 .js 源文件           磁盘 I/O
  → JS_Eval() 编译为字节码          词法分析 + 语法分析 + 代码生成
  → JS_NewRuntime()                 分配 atom 表、shape 表、GC 状态
  → JS_NewContext()                 分配内置对象、原型链
  → js_web_init() 注册 Web API      注册 6 个 class + 30 多个函数
  → JS_EvalFunction() 执行模块
  → JS_Call() 调用 handler
  → JS_RunGC()
  → JS_FreeContext() + JS_FreeRuntime()
```

然后给出优化建议，按影响排序。排第一的就是：用启动时已编译好的字节码，替代每次请求的重新编译。

它找到了。跟我看到的是同一个问题。

## 改动

我让它开始做。改动很小——把 `JS_Eval()` 从源码编译替换为 `JS_ReadObject()` 加载字节码：

```diff
-    size_t src_len;
-    char *src = js_qjs_read_file(rt->script_path, &src_len);
-    if (!src) goto fail;
-    JSValue obj = JS_Eval(qctx, src, src_len, rt->script_path,
-                          JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
-    free(src);
+    JSValue obj = JS_ReadObject(qctx, rt->bytecode, rt->bytecode_len,
+                                JS_READ_OBJ_BYTECODE);
     if (JS_IsException(obj)) goto fail;
```

5 行变 2 行。删掉文件 I/O，删掉编译，直接用内存里现成的字节码。

编译通过，跑测试——59 个过了，1 个段错误。用了 ES module import 的脚本崩了。

## 段错误

AI 没有猜原因，没有回退到老方案。它去读了 QuickJS 的源码，发现原来的 `JS_Eval()` 内部会自动调用 `js_resolve_module` 解析 import 依赖，而 `JS_ReadObject()` 加载字节码后不会。import 的模块指针为空，后续链接时解引用空指针，段错误。

AI 在 QuickJS 的公开 API 里找到了 `JS_ResolveModule`，补在字节码加载之后、执行之前。60 个测试全过。

代码变更: [6c0d7e7](https://github.com/hongzhidao/jsmock/commit/6c0d7e7)

## 去掉多余的 GC

字节码优化之后，AI 建议的第二项是去掉 `JS_RunGC()`。每次请求结束时，代码先跑一遍 GC，再释放 Context 和 Runtime：

```c
JS_RunGC(qrt);
JS_FreeContext(qctx);
JS_FreeRuntime(qrt);
```

`JS_FreeRuntime` 会销毁整个运行时的所有内存。在它之前跑 GC，就像拆迁之前先请保洁——不是慢不慢的问题，是这行代码不该存在。

它的存在说明写的人没有真正理解 `JS_FreeRuntime` 做了什么。看到"释放资源"就条件反射地加一个"先清理"——这是防御性编程的本能，但在这里是错的。而且它会误导读代码的人：看到 `JS_RunGC`，会以为这里有什么微妙的原因必须先跑 GC，然后花时间去想这个不存在的原因。

代码里有 4 处这样的调用，全部删掉，60 个测试通过。

代码变更: [e03bd55](https://github.com/hongzhidao/jsmock/commit/e03bd55)

## 测试和压测

每做一项优化，AI 自己跑全部 60 个测试确认功能正确，然后用 wrk 做优化前后的 A/B 压测——保存旧版二进制，编译新版，相同参数跑两轮对比。两项优化累计：

| 版本 | req/s | 延迟 |
|------|-------|------|
| 原始版本 | 13,869 | 3.39ms |
| 优化后 | 17,647 | 2.65ms |
| 累计提升 | **+27%** | **-22%** |

这类验证工作——跑测试、保存旧版、编译新版、用相同参数压测、对比数据——以前要么人手动做，要么专门写自动化脚本。现在 AI 自己就能把整个流程跑完，结果直接摆出来。这是 AI 最能体现效率的地方之一：不是写代码快，而是把验证这些繁琐但必要的工作变成了零成本。

## HTTP Keep-Alive

做完字节码和 GC 两项优化之后，AI 没有停。它往上走了一层——从 JS 执行路径看到 HTTP 连接层。

每次请求处理完，服务器直接关闭 TCP 连接：

```c
if (rc == 1) {
    /* fully written, close connection (no keep-alive for now) */
    js_conn_close(conn, &eng->epoll);
    js_conn_free(conn);
}
```

"no keep-alive for now"——这个注释是 AI 之前写 HTTP 层时自己留的。现在它主动回来补。

HTTP/1.1 默认 keep-alive：一条 TCP 连接上可以连续发多个请求。没有 keep-alive，每个请求都要走一遍 TCP 三次握手、accept、分配连接结构体、注册 epoll。这些开销跟业务逻辑毫无关系。

核心改动：写完响应后，不关连接，重置状态等下一个请求：

```diff
 if (rc == 1) {
-    /* fully written, close connection (no keep-alive for now) */
-    js_conn_close(conn, &eng->epoll);
-    js_conn_free(conn);
+    if (conn->keep_alive) {
+        conn->wbuf.len = 0;
+        conn->woff = 0;
+        conn->state = JS_CONN_READING;
+        js_epoll_mod(&eng->epoll, ev->fd, EPOLLIN, ev);
+        if (conn->rbuf.len > 0)
+            js_http_process(eng, conn);
+    } else {
+        js_conn_close(conn, &eng->epoll);
+        js_conn_free(conn);
+    }
 }
```

写缓冲区清零，写偏移归零，状态切回 READING，epoll 从 EPOLLOUT 改回 EPOLLIN。连接不释放，直接复用。

配套改动：响应必须始终带 `Content-Length`。没有 keep-alive 时可以不带——客户端读到 EOF 就知道结束了。但 keep-alive 连接不会 EOF，客户端必须靠 `Content-Length` 判断响应边界。少这个头，客户端会一直等。

## Pipelining

AI 写测试：在一条 TCP 连接上连发两个请求，验证能收到两个响应。测试失败——只收到一个。

原因：客户端把两个请求写进同一个 TCP segment。服务器第一次 `read()` 就把两个请求的数据都读进了 rbuf。处理完第一个请求、写回响应后，连接切回 EPOLLIN 等新数据。但第二个请求的数据已经在 rbuf 里——不在内核缓冲区，epoll 不会再触发。

修复：把请求处理逻辑从 `on_read` 里提取成独立的 `js_http_process()`，写完响应重置后，检查 rbuf 是否还有数据：

```c
if (conn->rbuf.len > 0)
    js_http_process(eng, conn);
```

这个问题不踩不会想到。epoll 只管内核缓冲区，你用户态 buffer 里攒了什么它不知道。

压测结果：

| 版本 | req/s | 延迟 | 连接错误 |
|------|-------|------|----------|
| keep-alive 前 | 19,687 | 479μs | 198,825 |
| keep-alive 后 | 22,205 | 460μs | 0 |
| 提升 | **+13%** | **-4%** | **-100%** |

吞吐量提升 13%。但最有意义的数字是连接错误：从 198,825 降到 0。wrk 默认用 keep-alive，之前每个请求都因为服务器关连接而报错重连。现在连接复用，零错误。

代码变更: [9b4a503](https://github.com/hongzhidao/jsmock/commit/9b4a503)

## AI 做性能优化，到底什么水平

三项优化，AI 全程自主完成：字节码、GC、keep-alive——一句提示，没有引导。

结果我是满意的。AI 发现了我没提的问题，改动干净，遇到段错误没有瞎试——读了 QuickJS 源码，找到根因。keep-alive 的 pipelining 问题也没绕过——分析了 epoll 和用户态缓冲的交互，提取函数，干净修复。从发现问题到修复到验证，全程没有引导。

三项优化累计，吞吐量从 13,869 到 22,205，+60%。但真正的大头还在——每次请求创建一个完整的 JSRuntime，注册 30 多个 Web API 绑定，重建整张路由表。这些加起来比编译、GC、连接握手都重。AI 把它们列在建议里，排在后面。这些留到后续继续。

这跟第六篇的结论是一回事。用字节码替代重编译、删掉多余的 GC、加上 keep-alive，都是在现有架构内消除浪费——战术优化。持久化 Runtime 是改架构——战略优化。AI 做战术优化很利落：发现问题、定位根因、干净修复。战略优化需要人来判断方向。

这篇让我对 AI 做性能优化有了更具体的感受。它是真的厉害——段错误不瞎猜，去读 QuickJS 源码找根因；pipelining 问题不绕过，分析出 epoll 和用户态缓冲的交互细节。这些不是背答案能解决的，需要理解底层原理。但哪些优化值得做、哪些需要改架构、改完之后系统往哪走——这些判断还是得人来。

至少这几年，人和 AI 的关系不是替代，是协作。AI 把执行成本降到接近零——写代码、跑测试、做压测、修 bug，以前一天的活现在几分钟。人把精力集中在方向和决策上。各干各擅长的事，效率比任何一方单独做都高。

jsmock 不会停在这里。我的目标是把它做成一个生产级别的开源 mock server——不是玩具，不是 demo，是真正能用在工作中的工具。从架构到性能到功能，一步一步来，每一步都有 AI 参与。这个项目本身就是一个实验：在 AI 的帮助下，一个人能把一个开源软件做到什么程度。

欢迎关注这个项目，看它怎么进化。

---

专栏：[我带 AI 写了个项目](https://www.zhihu.com/column/c_2006330352843657698)

GitHub: https://github.com/hongzhidao/jsmock
