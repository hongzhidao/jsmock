import { greet } from "./helper_module.js";

mock.get("/greet/:name", (req) => {
    return new Response(greet(req.params.name));
});

export default { listen: 18088 };
