mock.get("/hello", (req) => {
    return new Response("Hello, World!");
});

mock.get("/users/:id", (req) => {
    return new Response(JSON.stringify({ id: req.params.id }), {
        status: 200,
        headers: { "Content-Type": "application/json" }
    });
});

export default { listen: 3000 };
