#!/bin/sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
ASSETD_BIN="${ASSETD_BIN:-${ROOT_DIR}/build/assetd}"
INTERFACE="${CAPTURE_INTERFACE:-}"
HOST="${ASSETD_HOST:-127.0.0.1}"
PORT="${ASSETD_PORT:-18080}"
SQLITE_PATH="${SQLITE_PATH:-${ROOT_DIR}/live-capture-test.db}"
RUNTIME_LOG="${RUNTIME_LOG:-${ROOT_DIR}/logs/live-capture-test.log}"
FILTER="${CAPTURE_FILTER:-arp or udp port 67 or udp port 68 or udp port 1900 or udp port 5353}"
DURATION="${CAPTURE_DURATION:-20}"
POLL_INTERVAL="${POLL_INTERVAL:-2}"

fail() {
    echo "error: $*" >&2
    exit 1
}

need_command() {
    command -v "$1" >/dev/null 2>&1 || fail "missing required command: $1"
}

detect_interface() {
    ip route show default 2>/dev/null | awk 'NR == 1 {
        for (i = 1; i <= NF; i++) {
            if ($i == "dev") {
                print $(i + 1)
                exit
            }
        }
    }'
}

api() {
    curl --silent --show-error "$@"
}

generate_probe_traffic() {
    gateway="$(ip route show default 2>/dev/null | awk 'NR == 1 {
        for (i = 1; i <= NF; i++) {
            if ($i == "via") {
                print $(i + 1)
                exit
            }
        }
    }')"

    if [ -n "$gateway" ]; then
        echo "Generating probe traffic to default gateway: ${gateway}"
        ping -c 3 -W 1 "$gateway" >/dev/null 2>&1 || true
        if command -v arping >/dev/null 2>&1; then
            arping -c 3 -w 3 -I "$INTERFACE" "$gateway" >/dev/null 2>&1 || true
        fi
    else
        echo "No default gateway found; waiting for ambient ARP/DHCP traffic."
    fi
}

need_command curl
need_command ip
need_command awk
need_command ping

[ -x "$ASSETD_BIN" ] || fail "assetd binary not found or not executable: ${ASSETD_BIN}. Build first with: cmake --build build --parallel"

if [ -z "$INTERFACE" ]; then
    INTERFACE="$(detect_interface)"
fi
[ -n "$INTERFACE" ] || fail "could not detect interface. Set CAPTURE_INTERFACE=<iface>."

mkdir -p "$(dirname -- "$SQLITE_PATH")" "$(dirname -- "$RUNTIME_LOG")"

echo "Live capture test"
echo "  binary:       ${ASSETD_BIN}"
echo "  interface:    ${INTERFACE}"
echo "  endpoint:     http://${HOST}:${PORT}"
echo "  sqlite:       ${SQLITE_PATH}"
echo "  runtime log:  ${RUNTIME_LOG}"
echo "  filter:       ${FILTER}"
echo "  duration:     ${DURATION}s"
echo
echo "If capture start is rejected with a permission error, run assetd as root or grant:"
echo "  sudo setcap cap_net_raw,cap_net_admin=eip ${ASSETD_BIN}"
echo

"$ASSETD_BIN" \
    --serve \
    --listen-address "$HOST" \
    --port "$PORT" \
    --capture-mode live \
    --interface "$INTERFACE" \
    --sqlite "$SQLITE_PATH" \
    --runtime-log "$RUNTIME_LOG" \
    --filter "$FILTER" &
assetd_pid="$!"

cleanup() {
    api -X POST "http://${HOST}:${PORT}/api/v1/capture/stop" >/dev/null 2>&1 || true
    kill "$assetd_pid" >/dev/null 2>&1 || true
    wait "$assetd_pid" >/dev/null 2>&1 || true
}
trap cleanup EXIT INT TERM

ready=0
for _ in $(seq 1 30); do
    if api "http://${HOST}:${PORT}/api/v1/status" >/dev/null 2>&1; then
        ready=1
        break
    fi
    sleep 1
done
[ "$ready" -eq 1 ] || fail "assetd did not become ready"

echo "Starting live capture..."
api -X POST "http://${HOST}:${PORT}/api/v1/capture/start"
echo

generate_probe_traffic

end_time=$(( $(date +%s) + DURATION ))
while [ "$(date +%s)" -lt "$end_time" ]; do
    echo
    echo "Status:"
    api "http://${HOST}:${PORT}/api/v1/status"
    echo
    echo "Assets:"
    api "http://${HOST}:${PORT}/api/v1/assets"
    echo
    echo "Events:"
    api "http://${HOST}:${PORT}/api/v1/events?limit=10"
    echo
    sleep "$POLL_INTERVAL"
done

echo
echo "Stopping live capture..."
api -X POST "http://${HOST}:${PORT}/api/v1/capture/stop"
echo
echo
echo "Recent logs:"
api "http://${HOST}:${PORT}/api/v1/logs?limit=20"
echo
echo
echo "Done. SQLite database: ${SQLITE_PATH}"
