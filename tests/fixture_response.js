mock.get("/string", (req) => {
    return new Response("hello world");
});

mock.get("/empty", (req) => {
    return new Response(null, { status: 204 });
});

mock.get("/custom", (req) => {
    return new Response('{"ok":true}', {
        status: 201,
        headers: {
            "Content-Type": "application/json",
            "X-Custom": "test-value"
        }
    });
});

export default { listen: 18085 };
