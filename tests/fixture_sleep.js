// Top-level await sleep — routes registered after sleep completes
const sleep = (ms) => new Promise(r => setTimeout(r, ms));
await sleep(100);

mock.get("/hello", (req) => {
    return new Response("hello-after-sleep");
});

mock.get("/async", async (req) => {
    return new Promise((resolve) => {
        setTimeout(() => {
            resolve(new Response("async-after-sleep"));
        }, 50);
    });
});

export default { listen: 18094 };
