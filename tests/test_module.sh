#!/bin/bash
# Test: ES6 module import

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

echo "=== test_module ==="

$JSMOCK "$(dirname "$0")/fixture_module.js" 2>/dev/null &
PID=$!
sleep 1

BASE="http://127.0.0.1:18088"

# --- import { fn } from "./file.js" ---
BODY=$(curl -sf "$BASE/greet/world")
assert_eq "import function from module" "hello world" "$BODY"

# --- Summary ---
echo ""
echo "test_module: $PASS/$TESTS passed"
[ "$FAIL" -eq 0 ] || exit 1
