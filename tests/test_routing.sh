#!/bin/bash
# Test: Routing - HTTP methods and path parameters

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

echo "=== test_routing ==="

$JSMOCK "$(dirname "$0")/fixture_routing.js" 2>/dev/null &
PID=$!
sleep 1

BASE="http://127.0.0.1:18083"

# --- mock.get ---
BODY=$(curl -sf "$BASE/get")
assert_eq "mock.get" "GET OK" "$BODY"

# --- mock.post ---
BODY=$(curl -sf -X POST "$BASE/post")
assert_eq "mock.post" "POST OK" "$BODY"

# --- mock.put ---
BODY=$(curl -sf -X PUT "$BASE/put")
assert_eq "mock.put" "PUT OK" "$BODY"

# --- mock.patch ---
BODY=$(curl -sf -X PATCH "$BASE/patch")
assert_eq "mock.patch" "PATCH OK" "$BODY"

# --- mock.delete ---
BODY=$(curl -sf -X DELETE "$BASE/delete")
assert_eq "mock.delete" "DELETE OK" "$BODY"

# --- mock.all matches GET ---
BODY=$(curl -sf "$BASE/any")
assert_eq "mock.all matches GET" "ALL GET" "$BODY"

# --- mock.all matches POST ---
BODY=$(curl -sf -X POST "$BASE/any")
assert_eq "mock.all matches POST" "ALL POST" "$BODY"

# --- mock.all matches DELETE ---
BODY=$(curl -sf -X DELETE "$BASE/any")
assert_eq "mock.all matches DELETE" "ALL DELETE" "$BODY"

# --- Path parameter: single ---
BODY=$(curl -sf "$BASE/users/42")
assert_eq "path param :id" "user:42" "$BODY"

# --- Path parameter: multiple ---
BODY=$(curl -sf "$BASE/posts/7/comments/99")
assert_eq "path params :pid/:cid" "7:99" "$BODY"

# --- 404 for unmatched route ---
STATUS=$(curl -s -o /dev/null -w '%{http_code}' "$BASE/nonexistent")
assert_eq "404 for unmatched route" "404" "$STATUS"

# --- Summary ---
echo ""
echo "test_routing: $PASS/$TESTS passed"
[ "$FAIL" -eq 0 ] || exit 1
