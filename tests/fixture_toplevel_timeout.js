// Top-level timeouts — verify the runtime isn't freed prematurely
setTimeout(() => {}, 50);
setTimeout(() => {}, 100);

mock.get("/sync", (req) => {
    return new Response("sync-ok");
});

mock.get("/async", async (req) => {
    return new Promise((resolve) => {
        setTimeout(() => {
            resolve(new Response("async-ok"));
        }, 50);
    });
});

export default { listen: 18093 };
