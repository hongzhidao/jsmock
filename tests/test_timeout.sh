#!/bin/bash
# Test: setTimeout and async handler support

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

echo "=== test_timeout ==="

$JSMOCK "$(dirname "$0")/fixture_timeout.js" 2>/dev/null &
PID=$!
sleep 1

# --- Test 1: sync handler still works ---
echo "[1] sync handler"
BODY=$(curl -sf http://127.0.0.1:18090/sync)
assert_eq "GET /sync returns sync-ok" "sync-ok" "$BODY"

# --- Test 2: async handler that resolves immediately ---
echo "[2] async immediate handler"
BODY=$(curl -sf http://127.0.0.1:18090/async-immediate)
assert_eq "GET /async-immediate returns async-immediate-ok" "async-immediate-ok" "$BODY"

# --- Test 3: delayed response via setTimeout ---
echo "[3] delayed handler (100ms setTimeout)"
BODY=$(curl -sf --max-time 5 http://127.0.0.1:18090/delayed)
assert_eq "GET /delayed returns delayed-ok" "delayed-ok" "$BODY"

# --- Test 4: delayed with custom status and headers ---
echo "[4] delayed with custom status and headers"
RESP=$(curl -sf --max-time 5 -w '\n%{http_code}' http://127.0.0.1:18090/delayed-status)
BODY=$(echo "$RESP" | head -1)
STATUS=$(echo "$RESP" | tail -1)
assert_eq "GET /delayed-status body" "custom-body" "$BODY"
assert_eq "GET /delayed-status status 201" "201" "$STATUS"

HEADER=$(curl -sf --max-time 5 -D - -o /dev/null http://127.0.0.1:18090/delayed-status | grep -i "X-Custom:" | tr -d '\r')
assert_eq "GET /delayed-status X-Custom header" "X-Custom: hello" "$HEADER"

stop_server

# --- Summary ---
echo ""
echo "test_timeout: $PASS/$TESTS passed"
[ "$FAIL" -eq 0 ] || exit 1
