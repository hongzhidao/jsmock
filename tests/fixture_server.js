mock.get("/ping", (req) => {
    return new Response("pong");
});

export default { listen: 18080 };
