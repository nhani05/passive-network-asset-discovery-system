#!/bin/sh
set -eu

image_name="${IMAGE_NAME:-passive-asset-discovery}"
export IMAGE_NAME="${image_name}"
filter_expression="${CAPTURE_FILTER:-arp or udp port 67 or udp port 68}"
tmp_dir="$(mktemp -d)"

cleanup() {
    rm -rf "${tmp_dir}"
}
trap cleanup EXIT
chmod 777 "${tmp_dir}"

echo "Building Docker image: ${image_name}"
docker build -t "${image_name}" .

echo "Validating Docker Compose configuration"
docker compose config >/dev/null

echo "Running PCAP fixture inside the container (table output)"
docker run --rm \
    -v "$PWD/samples:/samples:ro" \
    -v "${tmp_dir}:/data" \
    -e SQLITE_DATABASE_PATH=/data/arp.db \
    "${image_name}" \
    --pcap /samples/arp.pcap \
    --filter "${filter_expression}" \
    --output table

test -s "${tmp_dir}/arp.db"

echo "Running PCAP fixture and writing discovered assets to SQLite"
docker run --rm \
    -v "$PWD/samples:/samples:ro" \
    -v "${tmp_dir}:/data" \
    -e SQLITE_DATABASE_PATH=/data/multi.db \
    "${image_name}" \
    --pcap /samples/multi-asset.pcap \
    --filter "${filter_expression}" \
    --output json

test -s "${tmp_dir}/multi.db"

echo "Running Compose PCAP demo with SQLite volume"
docker compose run --rm pcap-demo >/dev/null

echo "Docker runtime verification completed"
