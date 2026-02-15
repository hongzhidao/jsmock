mock.get("/ping", (req) => {
    return new Response("pong");
});

export default { listen: "127.0.0.1:18081" };
