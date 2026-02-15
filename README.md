# jsmock

A programmable HTTP mock server powered by **C + QuickJS + epoll**.

Write mock APIs in JavaScript, serve at native speed. Single binary, zero dependencies.

## Why jsmock?

Tools like Postman mock and json-server are convenient but limited — complex scenarios require workarounds or aren't possible at all. WireMock is powerful but heavyweight (JVM).

**jsmock** gives you both:

- **Native performance** — C core with epoll, multi-threaded, each request runs compiled bytecode
- **JavaScript flexibility** with standard Web APIs (`Request`, `Response`, `URL`, `Headers`) — write mock logic the same way you write browser or Deno code
- **Zero runtime dependencies** — single binary, QuickJS embedded, just build and run
- **Stateful mocks** — built-in C-side key-value store shared across requests

## Features

- **Web-standard APIs**: `Request`, `Response`, `URL`, `Headers`, `TextEncoder`/`TextDecoder`, `console`
- **Express-style routing**: `mock.get()`, `mock.post()`, `mock.all()` with path parameters (`:id`)
- **Stateful storage**: `mock.store.get/set/del/incr/clear` — state persists across isolated request contexts
- **Multi-threaded**: N worker threads, each with its own epoll event loop
- **ES modules**: split mock definitions across files with `import`/`export`

## Quick Start

```bash
git clone https://github.com/hongzhidao/jsmock.git
cd jsmock
make        # downloads and builds QuickJS automatically
```

### Requirements

- Linux (epoll-based)
- GCC or Clang
- Git (for fetching QuickJS)

## Usage

```bash
./jsmock mock.js
```

```
Options:
  -v, --version     Print version and exit
  -h, --help        Show this help
```

## Script Format

A mock script is an ES module. Register route handlers with `mock.*()`, configure the server with `export default`:

```js
mock.get("/api/users", () => {
  return new Response(JSON.stringify([{ id: 1, name: "Alice" }]));
});

mock.post("/api/users", (req) => {
  const user = req.json();
  return new Response(JSON.stringify(user), { status: 201 });
});

export default { listen: 8080 };
```

```bash
curl http://localhost:8080/api/users
curl -X POST http://localhost:8080/api/users \
  -H "Content-Type: application/json" \
  -d '{"name":"Bob"}'
```

See [docs/](docs/) for the full API reference.

## How it was built

The first version of jsmock was built entirely through AI-assisted development — the human provided architecture decisions and requirements, the AI (Claude Opus 4.6 via [Claude Code](https://claude.ai/claude-code)) wrote all the code.

This project is actively maintained. Issues and pull requests are welcome.

## License

MIT
