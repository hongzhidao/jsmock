mock.post("/store/set", (req) => {
    const body = req.json();
    mock.store.set(body.key, body.value);
    return new Response("ok");
});

mock.get("/store/get/:key", (req) => {
    const val = mock.store.get(req.params.key);
    return new Response(val !== undefined ? JSON.stringify(val) : "null");
});

mock.post("/store/del", (req) => {
    const body = req.json();
    mock.store.del(body.key);
    return new Response("ok");
});

mock.post("/store/incr", (req) => {
    const body = req.json();
    const val = mock.store.incr(body.key);
    return new Response(String(val));
});

mock.post("/store/clear", (req) => {
    mock.store.clear();
    return new Response("ok");
});

export default { listen: 18086 };
