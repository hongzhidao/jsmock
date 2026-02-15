mock.get("/url-parse", (req) => {
    const u = new URL("http://example.com:8080/path?foo=bar&baz=1#frag");
    return new Response(JSON.stringify({
        protocol: u.protocol,
        hostname: u.hostname,
        port: u.port,
        pathname: u.pathname,
        search: u.search,
        hash: u.hash
    }));
});

mock.get("/url-searchparams", (req) => {
    const u = new URL("http://example.com/path?a=1&b=2&a=3");
    const sp = u.searchParams;
    return new Response(JSON.stringify({
        a: sp.get("a"),
        b: sp.get("b"),
        missing: sp.get("missing")
    }));
});

mock.get("/headers-api", (req) => {
    const h = new Headers();
    h.set("X-Foo", "bar");
    h.set("X-Baz", "qux");
    const hasFoo = h.has("X-Foo");
    const val = h.get("X-Foo");
    h.delete("X-Baz");
    const hasBaz = h.has("X-Baz");
    return new Response(JSON.stringify({
        hasFoo: hasFoo,
        val: val,
        hasBaz: hasBaz
    }));
});

mock.get("/textencoder", (req) => {
    const enc = new TextEncoder();
    const buf = enc.encode("hello");
    return new Response(JSON.stringify({
        length: buf.length,
        bytes: Array.from(buf)
    }));
});

mock.get("/textdecoder", (req) => {
    const enc = new TextEncoder();
    const buf = enc.encode("world");
    const dec = new TextDecoder();
    const str = dec.decode(buf);
    return new Response(str);
});

mock.get("/console-test", (req) => {
    console.log("test-log-output");
    return new Response("logged");
});

export default { listen: 18087 };
