#!/bin/bash
# Test: Web APIs - URL, Headers, TextEncoder, TextDecoder, console.log

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

assert_contains() {
    local desc="$1" needle="$2" haystack="$3"
    TESTS=$((TESTS + 1))
    if echo "$haystack" | grep -qF "$needle"; then
        echo "  PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $desc (expected to contain '$needle', got='$haystack')"
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

echo "=== test_webapi ==="

STDERR_LOG=$(mktemp)
$JSMOCK "$(dirname "$0")/fixture_webapi.js" 2>"$STDERR_LOG" &
PID=$!
sleep 1

BASE="http://127.0.0.1:18087"

# --- URL parsing ---
BODY=$(curl -sf "$BASE/url-parse")
assert_contains "URL.protocol" '"protocol":"http:"' "$BODY"
assert_contains "URL.hostname" '"hostname":"example.com"' "$BODY"
assert_contains "URL.port" '"port":"8080"' "$BODY"
assert_contains "URL.pathname" '"pathname":"/path"' "$BODY"
assert_contains "URL.search" '"search":"?foo=bar&baz=1"' "$BODY"
assert_contains "URL.hash" '"hash":"#frag"' "$BODY"

# --- URLSearchParams ---
BODY=$(curl -sf "$BASE/url-searchparams")
assert_contains "searchParams.get(a)" '"a":"1"' "$BODY"
assert_contains "searchParams.get(b)" '"b":"2"' "$BODY"
assert_contains "searchParams.get(missing)" '"missing":null' "$BODY"

# --- Headers API ---
BODY=$(curl -sf "$BASE/headers-api")
assert_contains "Headers.has true" '"hasFoo":true' "$BODY"
assert_contains "Headers.get" '"val":"bar"' "$BODY"
assert_contains "Headers.delete" '"hasBaz":false' "$BODY"

# --- TextEncoder ---
BODY=$(curl -sf "$BASE/textencoder")
assert_contains "TextEncoder length" '"length":5' "$BODY"
assert_contains "TextEncoder bytes" '"bytes":[104,101,108,108,111]' "$BODY"

# --- TextDecoder ---
BODY=$(curl -sf "$BASE/textdecoder")
assert_eq "TextDecoder decode" "world" "$BODY"

# --- console.log ---
curl -sf "$BASE/console-test" > /dev/null
sleep 0.3
assert_contains "console.log output" "test-log-output" "$(cat "$STDERR_LOG")"

# --- Summary ---
echo ""
echo "test_webapi: $PASS/$TESTS passed"
rm -f "$STDERR_LOG"
[ "$FAIL" -eq 0 ] || exit 1
