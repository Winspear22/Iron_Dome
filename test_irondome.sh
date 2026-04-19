#!/bin/bash

BINARY="./irondome"
TEST_DIR="/tmp/irondome_test"
LOG_FILE="/var/log/irondome/irondome.log"
PASS="[PASS]"
FAIL="[FAIL]"
ERRORS=0

check_log()
{
    local keyword="$1"
    local desc="$2"
    if grep -q "$keyword" "$LOG_FILE"; then
        echo "$PASS $desc"
    else
        echo "$FAIL $desc"
        ERRORS=$((ERRORS + 1))
    fi
}

cleanup()
{
    pkill -x irondome 2>/dev/null
    rm -rf "$TEST_DIR"
}
trap cleanup EXIT

# ── Pre-checks ────────────────────────────────────────────────────────────────

if [ "$(id -u)" -ne 0 ]; then
    echo "Error: this test suite must be run as root."
    exit 1
fi

if [ ! -f "$BINARY" ]; then
    echo "Error: $BINARY not found. Run 'make' first."
    exit 1
fi

pkill -x irondome 2>/dev/null
sleep 1

mkdir -p "$TEST_DIR"
mkdir -p "$(dirname $LOG_FILE)"
> "$LOG_FILE"

echo "==============================="
echo "    IRONDOME TEST SUITE"
echo "==============================="

# ── TEST 1: root permission check ─────────────────────────────────────────────

echo ""
echo "--- TEST 1: root permission ---"
sudo -u nobody "$BINARY" "$TEST_DIR" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "$PASS non-root execution correctly refused"
else
    echo "$FAIL should have refused non-root execution"
    ERRORS=$((ERRORS + 1))
fi

# ── TEST 2: daemon launch ─────────────────────────────────────────────────────

echo ""
echo "--- TEST 2: daemon launch ---"
"$BINARY" "$TEST_DIR"
sleep 1
DAEMON_PID=$(pgrep -x irondome)
if [ -n "$DAEMON_PID" ]; then
    echo "$PASS daemon is running (PID: $DAEMON_PID)"
else
    echo "$FAIL daemon is not running"
    ERRORS=$((ERRORS + 1))
    exit 1
fi
check_log "INFO: irondome started" "startup logged"

# ── TEST 3: disk read abuse ───────────────────────────────────────────────────

echo ""
echo "--- TEST 3: disk read abuse ---"
RFILE="$TEST_DIR/readtest.txt"
echo "irondome read abuse test data" > "$RFILE"
sleep 1
COUNT=0
while [ $COUNT -lt 60 ]; do
    cat "$RFILE" > /dev/null
    COUNT=$((COUNT + 1))
done
sleep 2
check_log "ALERT: read abuse" "read abuse detected after 60 reads in <10s"

# ── TEST 4: entropy spike ─────────────────────────────────────────────────────

echo ""
echo "--- TEST 4: entropy spike ---"
EFILE="$TEST_DIR/entropytest.bin"
# Low entropy baseline: repeated characters
python3 -c "print('a' * 8192, end='')" > "$EFILE"
sleep 3
# High entropy: overwrite with random bytes → triggers IN_MODIFY + entropy spike
dd if=/dev/urandom of="$EFILE" bs=4096 count=16 2>/dev/null
sleep 3
check_log "ALERT: entropy" "entropy spike detected after writing random data"

# ── TEST 5: cryptographic activity ───────────────────────────────────────────

echo ""
echo "--- TEST 5: cryptographic activity ---"
# Drain the kernel entropy pool to trigger the detection
dd if=/dev/urandom of=/dev/null bs=1024 count=2048 2>/dev/null
sleep 7
check_log "ALERT: crypto activity" "crypto activity detected after entropy pool drain"

# ── TEST 6: memory usage < 100 MB ────────────────────────────────────────────

echo ""
echo "--- TEST 6: memory usage ---"
DAEMON_PID=$(pgrep -x irondome)
if [ -n "$DAEMON_PID" ]; then
    MEM_KB=$(grep VmRSS /proc/$DAEMON_PID/status 2>/dev/null | awk '{print $2}')
    if [ -n "$MEM_KB" ] && [ "$MEM_KB" -lt 102400 ]; then
        echo "$PASS memory: ${MEM_KB} KB (limit: 102400 KB)"
    else
        echo "$FAIL memory: ${MEM_KB} KB exceeds 100 MB limit"
        ERRORS=$((ERRORS + 1))
    fi
else
    echo "$FAIL daemon not found for memory check"
    ERRORS=$((ERRORS + 1))
fi

# ── Results ───────────────────────────────────────────────────────────────────

echo ""
echo "==============================="
if [ $ERRORS -eq 0 ]; then
    echo "  ALL TESTS PASSED"
else
    echo "  $ERRORS TEST(S) FAILED"
fi
echo "  Full log: $LOG_FILE"
echo "==============================="
exit $ERRORS
