#!/bin/bash
# Test: Request API - method, url, headers, text(), json(), params

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
    fi
}
trap stop_server EXIT

echo "=== test_request ==="

$JSMOCK "$(dirname "$0")/fixture_request.js" 2>/dev/null &
PID=$!
sleep 1

BASE="http://127.0.0.1:18084"

# --- req.method ---
BODY=$(curl -sf "$BASE/method")
assert_eq "req.method GET" "GET" "$BODY"

BODY=$(curl -sf -X POST "$BASE/method")
assert_eq "req.method POST" "POST" "$BODY"

BODY=$(curl -sf -X PUT "$BASE/method")
assert_eq "req.method PUT" "PUT" "$BODY"

# --- req.url ---
BODY=$(curl -sf "$BASE/url")
assert_eq "req.url" "/url" "$BODY"

# --- req.headers.get ---
BODY=$(curl -sf -H "X-Custom: my-value" "$BASE/header")
assert_eq "req.headers.get" "my-value" "$BODY"

BODY=$(curl -sf "$BASE/header")
assert_eq "req.headers.get missing" "none" "$BODY"

# --- req.text() ---
BODY=$(curl -sf -X POST -d "hello body" "$BASE/text")
assert_eq "req.text()" "text:hello body" "$BODY"

# --- req.json() ---
BODY=$(curl -sf -X POST -H "Content-Type: application/json" -d '{"name":"alice"}' "$BASE/json")
assert_eq "req.json()" "name:alice" "$BODY"

# --- req.params ---
BODY=$(curl -sf "$BASE/params/123")
assert_eq "req.params" '{"id":"123"}' "$BODY"

# --- Summary ---
echo ""
echo "test_request: $PASS/$TESTS passed"
[ "$FAIL" -eq 0 ] || exit 1
