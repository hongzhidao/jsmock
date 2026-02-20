mock.get("/sync", (req) => {
    return new Response("sync-ok");
});

mock.get("/async-immediate", async (req) => {
    return new Response("async-immediate-ok");
});

mock.get("/delayed", async (req) => {
    return new Promise((resolve) => {
        setTimeout(() => {
            resolve(new Response("delayed-ok"));
        }, 100);
    });
});

mock.get("/delayed-status", async (req) => {
    return new Promise((resolve) => {
        setTimeout(() => {
            resolve(new Response("custom-body", {
                status: 201,
                headers: { "X-Custom": "hello" }
            }));
        }, 100);
    });
});

export default { listen: 18090 };
