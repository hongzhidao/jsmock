# API Reference

## Server Configuration

`export default` declares the server config. `listen` sets the address:

```js
// Port only (host defaults to 0.0.0.0)
export default { listen: 8080 };

// Host + port
export default { listen: "127.0.0.1:8080" };

// From environment
export default { listen: Number(mock.env("PORT")) || 3000 };
```

## Routes

```js
mock.get("/path", handler);
mock.post("/path", handler);
mock.put("/path", handler);
mock.patch("/path", handler);
mock.delete("/path", handler);
mock.all("/path", handler);       // any method
```

Handler is a function `(req) => Response`.

### Path Parameters

```js
mock.get("/api/users/:id/posts/:postId", (req) => {
  return new Response(JSON.stringify(req.params));  // { id: "42", postId: "7" }
});
```

## Request

Standard [Request](https://developer.mozilla.org/en-US/docs/Web/API/Request) Web API, plus `params` for path parameters:

```js
mock.post("/api/users/:id", (req) => {
  req.method;                              // "POST"
  req.url;                                 // "http://localhost:8080/api/users/42?page=1"
  req.headers.get("content-type");         // "application/json"
  req.text();                              // body as string
  req.json();                              // parsed JSON body

  // Extension: path params from router
  req.params;                              // { id: "42" }

  // Standard Web APIs work naturally
  const url = new URL(req.url);
  url.pathname;                            // "/api/users/42"
  url.searchParams.get("page");            // "1"

  return new Response(JSON.stringify(req.json()));
});
```

## Response

Standard [Response](https://developer.mozilla.org/en-US/docs/Web/API/Response/Response) Web API:

```js
new Response(JSON.stringify({ key: "value" }))   // JSON body
new Response("hello")                             // Plain text
new Response(null, { status: 204 })               // Empty

// Set status and headers
new Response(body, {
  status: 201,
  headers: { "Content-Type": "application/json" },
})
```

## Web APIs

Standard JavaScript Web APIs are available in your scripts:

```js
// URL / URLSearchParams
const url = new URL("https://example.com/path?q=test");
url.searchParams.get("q");         // "test"

// Headers
const h = new Headers();
h.set("X-Custom", "value");
h.get("X-Custom");                 // "value"

// TextEncoder / TextDecoder
new TextEncoder().encode("hello"); // Uint8Array
new TextDecoder().decode(bytes);   // string

// console
console.log("debug info");
```

## Stateful Mocks

Each request runs in an isolated JS context. Use `mock.store` to share state across requests (C-side key-value store):

```js
mock.get("/api/users", () => {
  const users = mock.store.get("users") || [];
  return new Response(JSON.stringify(users));
});

mock.post("/api/users", (req) => {
  const users = mock.store.get("users") || [];
  const id = mock.store.incr("nextId");
  const user = { id, ...req.json() };
  users.push(user);
  mock.store.set("users", users);
  return new Response(JSON.stringify(user), { status: 201 });
});

mock.delete("/api/users/:id", (req) => {
  const users = mock.store.get("users") || [];
  const idx = users.findIndex((u) => u.id === Number(req.params.id));
  if (idx === -1) return new Response(null, { status: 404 });
  users.splice(idx, 1);
  mock.store.set("users", users);
  return new Response(null, { status: 204 });
});
```

### Store API

```js
mock.store.get(key);          // Read value
mock.store.set(key, value);   // Write value (JSON-serializable)
mock.store.del(key);          // Delete key
mock.store.incr(key);         // Atomic increment, returns new value
mock.store.clear();           // Clear all
```

## Module Import

Split mock definitions across files, import shared logic:

```js
// helpers.js
export function paginate(items, url) {
  const params = new URL(url).searchParams;
  const page = Number(params.get("page")) || 1;
  const size = Number(params.get("size")) || 10;
  return {
    data: items.slice((page - 1) * size, page * size),
    total: items.length,
    page,
  };
}

// mock.js
import { paginate } from "./helpers.js";

const items = Array.from({ length: 100 }, (_, i) => ({ id: i + 1 }));

mock.get("/api/items", (req) => {
  return new Response(JSON.stringify(paginate(items, req.url)));
});

export default { listen: 3000 };
```
