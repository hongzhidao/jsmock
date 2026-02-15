mock.get("/get", (req) => {
    return new Response("GET OK");
});

mock.post("/post", (req) => {
    return new Response("POST OK");
});

mock.put("/put", (req) => {
    return new Response("PUT OK");
});

mock.patch("/patch", (req) => {
    return new Response("PATCH OK");
});

mock.delete("/delete", (req) => {
    return new Response("DELETE OK");
});

mock.all("/any", (req) => {
    return new Response("ALL " + req.method);
});

mock.get("/users/:id", (req) => {
    return new Response("user:" + req.params.id);
});

mock.get("/posts/:pid/comments/:cid", (req) => {
    return new Response(req.params.pid + ":" + req.params.cid);
});

export default { listen: 18083 };
