#!/bin/bash
# Test: Store API - get, set, del, incr, clear, persistence across requests

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

echo "=== test_store ==="

$JSMOCK "$(dirname "$0")/fixture_store.js" 2>/dev/null &
PID=$!
sleep 1

BASE="http://127.0.0.1:18086"

# --- mock.store.get on missing key returns null ---
BODY=$(curl -sf "$BASE/store/get/missing")
assert_eq "store.get missing key" "null" "$BODY"

# --- mock.store.set + mock.store.get ---
curl -sf -X POST -H "Content-Type: application/json" -d '{"key":"name","value":"alice"}' "$BASE/store/set" > /dev/null
BODY=$(curl -sf "$BASE/store/get/name")
assert_eq "store.set then store.get" '"alice"' "$BODY"

# --- state persists across requests ---
curl -sf -X POST -H "Content-Type: application/json" -d '{"key":"count","value":100}' "$BASE/store/set" > /dev/null
BODY=$(curl -sf "$BASE/store/get/count")
assert_eq "state persists across requests" '100' "$BODY"

# --- mock.store.del ---
curl -sf -X POST -H "Content-Type: application/json" -d '{"key":"name"}' "$BASE/store/del" > /dev/null
BODY=$(curl -sf "$BASE/store/get/name")
assert_eq "store.del removes key" "null" "$BODY"

# --- mock.store.incr ---
curl -sf -X POST -H "Content-Type: application/json" -d '{"key":"counter"}' "$BASE/store/incr" > /dev/null
BODY=$(curl -sf -X POST -H "Content-Type: application/json" -d '{"key":"counter"}' "$BASE/store/incr")
assert_eq "store.incr second call" "2" "$BODY"

BODY=$(curl -sf -X POST -H "Content-Type: application/json" -d '{"key":"counter"}' "$BASE/store/incr")
assert_eq "store.incr third call" "3" "$BODY"

# --- mock.store.clear ---
curl -sf -X POST "$BASE/store/clear" > /dev/null
BODY=$(curl -sf "$BASE/store/get/counter")
assert_eq "store.clear removes all" "null" "$BODY"

BODY=$(curl -sf "$BASE/store/get/count")
assert_eq "store.clear removes all (2)" "null" "$BODY"

# --- Summary ---
echo ""
echo "test_store: $PASS/$TESTS passed"
[ "$FAIL" -eq 0 ] || exit 1
