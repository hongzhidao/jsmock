#!/bin/bash
# Run all jsmock tests
set -e

DIR="$(dirname "$0")"
TOTAL_PASS=0
TOTAL_FAIL=0
FAILED_SUITES=""

# Kill any leftover jsmock processes from previous runs
pkill -x jsmock 2>/dev/null || true
sleep 0.3

for test in "$DIR"/test_*.sh; do
    echo ""
    if bash "$test"; then
        : # suite passed
    else
        FAILED_SUITES="$FAILED_SUITES $(basename "$test")"
    fi
    echo "---"
done

echo ""
if [ -z "$FAILED_SUITES" ]; then
    echo "ALL TEST SUITES PASSED"
else
    echo "FAILED SUITES:$FAILED_SUITES"
    exit 1
fi
