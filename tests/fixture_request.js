mock.all("/method", (req) => {
    return new Response(req.method);
});

mock.all("/url", (req) => {
    return new Response(req.url);
});

mock.all("/header", (req) => {
    const val = req.headers.get("X-Custom");
    return new Response(val || "none");
});

mock.post("/text", (req) => {
    const body = req.text();
    return new Response("text:" + body);
});

mock.post("/json", (req) => {
    const obj = req.json();
    return new Response("name:" + obj.name);
});

mock.get("/params/:id", (req) => {
    return new Response(JSON.stringify(req.params));
});

export default { listen: 18084 };
