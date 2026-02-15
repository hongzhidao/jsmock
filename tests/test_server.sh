#!/bin/bash
# Test: Server startup and listen configuration

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

echo "=== test_server ==="

# --- Test 1: jsmock starts and accepts connections (port number) ---
echo "[1] jsmock mock.js starts server"
$JSMOCK "$(dirname "$0")/fixture_server.js" 2>/dev/null &
PID=$!
sleep 1

BODY=$(curl -sf http://127.0.0.1:18080/ping)
assert_eq "GET /ping returns pong" "pong" "$BODY"

stop_server

# --- Test 2: listen with host:port string ---
echo "[2] export default { listen: '127.0.0.1:18081' }"
$JSMOCK "$(dirname "$0")/fixture_server_host.js" 2>/dev/null &
PID=$!
sleep 1

BODY=$(curl -sf http://127.0.0.1:18081/ping)
assert_eq "host:port binding works" "pong" "$BODY"

stop_server

# --- Test 3: mock.env() reads env variable ---
echo "[3] mock.env('TEST_VAR')"
TEST_VAR="hello_env" $JSMOCK "$(dirname "$0")/fixture_env.js" 2>/dev/null &
PID=$!
sleep 1

BODY=$(curl -sf http://127.0.0.1:18082/env)
assert_eq "mock.env reads TEST_VAR" "hello_env" "$BODY"

stop_server

# --- Summary ---
echo ""
echo "test_server: $PASS/$TESTS passed"
[ "$FAIL" -eq 0 ] || exit 1
