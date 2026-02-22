#!/bin/bash
# Test: top-level setTimeout doesn't crash the server

JSMOCK="$(dirname "$0")/../jsmock"
PASS=0
FAIL=0
TESTS=0

assert_eq() {
    local desc="$1" expected="$2" actual="$3"
    TESTS=$((TESTS + 1))
    if [ "$expected" = "$actual" ]; then
        echo "  PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $desc (expected='$expected', got='$actual')"
        FAIL=$((FAIL + 1))
    fi
}

stop_server() {
    if [ -n "$PID" ]; then
        kill "$PID" 2>/dev/null
        wait "$PID" 2>/dev/null || true
        PID=
        sleep 0.3
    fi
}
trap stop_server EXIT

echo "=== test_toplevel_timeout ==="

$JSMOCK "$(dirname "$0")/fixture_toplevel_timeout.js" 2>/dev/null &
PID=$!
sleep 1

# --- Test 1: sync handler with top-level setTimeout ---
echo "[1] sync handler with top-level setTimeout"
BODY=$(curl -sf --max-time 5 http://127.0.0.1:18093/sync)
assert_eq "GET /sync returns sync-ok" "sync-ok" "$BODY"

# --- Test 2: async handler with top-level setTimeout ---
echo "[2] async handler with top-level setTimeout"
BODY=$(curl -sf --max-time 5 http://127.0.0.1:18093/async)
assert_eq "GET /async returns async-ok" "async-ok" "$BODY"

# --- Test 3: 404 with top-level setTimeout ---
echo "[3] 404 with top-level setTimeout"
STATUS=$(curl -s --max-time 5 -o /dev/null -w '%{http_code}' http://127.0.0.1:18093/nonexistent)
assert_eq "GET /nonexistent returns 404" "404" "$STATUS"

# --- Test 4: multiple sequential requests (no leak/crash) ---
echo "[4] multiple sequential requests"
for i in 1 2 3 4 5; do
    BODY=$(curl -sf --max-time 5 http://127.0.0.1:18093/sync)
    if [ "$BODY" != "sync-ok" ]; then
        assert_eq "sequential request $i" "sync-ok" "$BODY"
        break
    fi
done
assert_eq "5 sequential requests all returned sync-ok" "sync-ok" "$BODY"

stop_server

# --- Summary ---
echo ""
echo "test_toplevel_timeout: $PASS/$TESTS passed"
[ "$FAIL" -eq 0 ] || exit 1
