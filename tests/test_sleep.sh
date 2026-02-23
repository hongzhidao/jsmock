#!/bin/bash
# Test: top-level await sleep() defers route registration

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

echo "=== test_sleep ==="

$JSMOCK "$(dirname "$0")/fixture_sleep.js" 2>/dev/null &
PID=$!
sleep 1

# --- Test 1: sync handler after top-level await sleep ---
echo "[1] sync handler after top-level await sleep"
BODY=$(curl -sf --max-time 5 http://127.0.0.1:18094/hello)
assert_eq "GET /hello returns hello-after-sleep" "hello-after-sleep" "$BODY"

# --- Test 2: async handler after top-level await sleep ---
echo "[2] async handler after top-level await sleep"
BODY=$(curl -sf --max-time 5 http://127.0.0.1:18094/async)
assert_eq "GET /async returns async-after-sleep" "async-after-sleep" "$BODY"

# --- Test 3: 404 for unknown route ---
echo "[3] 404 for unknown route"
STATUS=$(curl -s --max-time 5 -o /dev/null -w '%{http_code}' http://127.0.0.1:18094/nonexistent)
assert_eq "GET /nonexistent returns 404" "404" "$STATUS"

# --- Test 4: multiple sequential requests ---
echo "[4] multiple sequential requests"
for i in 1 2 3 4 5; do
    BODY=$(curl -sf --max-time 5 http://127.0.0.1:18094/hello)
    if [ "$BODY" != "hello-after-sleep" ]; then
        assert_eq "sequential request $i" "hello-after-sleep" "$BODY"
        break
    fi
done
assert_eq "5 sequential requests all returned hello-after-sleep" "hello-after-sleep" "$BODY"

stop_server

# --- Summary ---
echo ""
echo "test_sleep: $PASS/$TESTS passed"
[ "$FAIL" -eq 0 ] || exit 1
