#!/bin/sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
IMAGE_NAME="${IMAGE_NAME:-passive-asset-discovery}"
INTERFACE="${CAPTURE_INTERFACE:-}"
HOST="${ASSETD_HOST:-127.0.0.1}"
PORT="${ASSETD_PORT:-18080}"
FILTER="${CAPTURE_FILTER:-arp or udp port 67 or udp port 68 or udp port 1900 or udp port 5353}"
DURATION="${CAPTURE_DURATION:-20}"
POLL_INTERVAL="${POLL_INTERVAL:-2}"
DATA_DIR="${DATA_DIR:-${ROOT_DIR}/.live-capture-docker}"

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
need_command docker
need_command ip
need_command awk
need_command ping

if [ -z "$INTERFACE" ]; then
    INTERFACE="$(detect_interface)"
fi
[ -n "$INTERFACE" ] || fail "could not detect interface. Set CAPTURE_INTERFACE=<iface>."

mkdir -p "$DATA_DIR"

echo "Building Docker image: ${IMAGE_NAME}"
docker build -t "$IMAGE_NAME" "$ROOT_DIR"

echo "Live capture Docker test"
echo "  image:        ${IMAGE_NAME}"
echo "  interface:    ${INTERFACE}"
echo "  endpoint:     http://${HOST}:${PORT}"
echo "  data dir:     ${DATA_DIR}"
echo "  filter:       ${FILTER}"
echo "  duration:     ${DURATION}s"
echo

container_id="$(docker run -d --rm \
    --name pnad-live-capture-test \
    --network host \
    --cap-add NET_RAW \
    --cap-add NET_ADMIN \
    --user root \
    -v "${DATA_DIR}:/data" \
    --entrypoint assetd \
    "$IMAGE_NAME" \
    --serve \
    --listen-address "$HOST" \
    --port "$PORT" \
    --capture-mode live \
    --interface "$INTERFACE" \
    --sqlite /data/live-capture-test.db \
    --runtime-log /data/live-capture-test.log \
    --filter "$FILTER")"

cleanup() {
    api -X POST "http://${HOST}:${PORT}/api/v1/capture/stop" >/dev/null 2>&1 || true
    docker stop "$container_id" >/dev/null 2>&1 || true
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
[ "$ready" -eq 1 ] || fail "assetd container did not become ready"

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
echo "Container logs:"
docker logs "$container_id" || true
echo
echo "Done. Data directory: ${DATA_DIR}"
