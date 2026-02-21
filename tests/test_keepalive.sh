#!/bin/bash
# Test: HTTP keep-alive connection reuse

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

echo "=== test_keepalive ==="

$JSMOCK "$(dirname "$0")/fixture_server.js" 2>/dev/null &
PID=$!
sleep 1

# --- Test 1: two requests on one connection (keep-alive) ---
echo "[1] two requests on one keep-alive connection"
RESPONSE=$(printf "GET /ping HTTP/1.1\r\nHost: localhost\r\n\r\nGET /ping HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n" | nc -w 2 127.0.0.1 18080)
COUNT=$(echo "$RESPONSE" | grep -c "HTTP/1.1 200")
assert_eq "two responses on one connection" "2" "$COUNT"

# --- Test 2: response includes Connection: keep-alive ---
echo "[2] response includes Connection: keep-alive"
HAS_KA=$(echo "$RESPONSE" | grep -c "Connection: keep-alive")
assert_eq "Connection: keep-alive in response" "1" "$HAS_KA"

# --- Test 3: response includes Connection: close when requested ---
echo "[3] response includes Connection: close"
HAS_CLOSE=$(echo "$RESPONSE" | grep -c "Connection: close")
assert_eq "Connection: close in response" "1" "$HAS_CLOSE"

# --- Test 4: three pipelined requests ---
echo "[4] three requests on one connection"
RESPONSE=$(printf "GET /ping HTTP/1.1\r\nHost: localhost\r\n\r\nGET /ping HTTP/1.1\r\nHost: localhost\r\n\r\nGET /ping HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n" | nc -w 2 127.0.0.1 18080)
COUNT=$(echo "$RESPONSE" | grep -c "HTTP/1.1 200")
assert_eq "three responses on one connection" "3" "$COUNT"

# --- Test 5: Connection: close on first request closes immediately ---
echo "[5] Connection: close closes after first response"
RESPONSE=$(printf "GET /ping HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n" | nc -w 2 127.0.0.1 18080)
COUNT=$(echo "$RESPONSE" | grep -c "HTTP/1.1 200")
assert_eq "single response with Connection: close" "1" "$COUNT"

# --- Test 6: Content-Length always present ---
echo "[6] Content-Length in response"
HAS_CL=$(echo "$RESPONSE" | grep -c "Content-Length:")
assert_eq "Content-Length present" "1" "$HAS_CL"

stop_server

# --- Summary ---
echo ""
echo "test_keepalive: $PASS/$TESTS passed"
[ "$FAIL" -eq 0 ] || exit 1
