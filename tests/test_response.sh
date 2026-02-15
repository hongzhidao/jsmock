#!/bin/bash
# Test: Response API - string body, empty body, custom status and headers

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

echo "=== test_response ==="

$JSMOCK "$(dirname "$0")/fixture_response.js" 2>/dev/null &
PID=$!
sleep 1

BASE="http://127.0.0.1:18085"

# --- new Response(body) with string ---
BODY=$(curl -sf "$BASE/string")
assert_eq "Response(body) string" "hello world" "$BODY"

# --- new Response(null, { status: 204 }) ---
STATUS=$(curl -s -o /dev/null -w '%{http_code}' "$BASE/empty")
assert_eq "Response(null, {status:204})" "204" "$STATUS"

# --- new Response(body, { status, headers }) ---
STATUS=$(curl -s -o /dev/null -w '%{http_code}' "$BASE/custom")
assert_eq "custom status 201" "201" "$STATUS"

BODY=$(curl -sf "$BASE/custom")
assert_eq "custom body" '{"ok":true}' "$BODY"

CT=$(curl -sf -o /dev/null -w '%{content_type}' "$BASE/custom")
assert_eq "custom Content-Type" "application/json" "$CT"

XCUSTOM=$(curl -sf -D - -o /dev/null "$BASE/custom" 2>/dev/null | grep -i "X-Custom:" | tr -d '\r' | awk '{print $2}')
assert_eq "custom X-Custom header" "test-value" "$XCUSTOM"

# --- Summary ---
echo ""
echo "test_response: $PASS/$TESTS passed"
[ "$FAIL" -eq 0 ] || exit 1
