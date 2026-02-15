mock.get("/env", (req) => {
    const val = mock.env("TEST_VAR");
    return new Response(val || "undefined");
});

export default { listen: 18082 };
