#!/bin/sh
set -eu

image_name="${IMAGE_NAME:-passive-asset-discovery}"
filter_expression="${CAPTURE_FILTER:-arp or udp port 67 or udp port 68}"

echo "Building Docker image: ${image_name}"
docker build -t "${image_name}" .

echo "Running PCAP fixture inside the container"
docker run --rm \
    -v "$PWD/samples:/samples:ro" \
    "${image_name}" \
    --pcap /samples/arp.pcap \
    --filter "${filter_expression}" \
    --output table

echo "Validating Docker Compose configuration"
docker compose config >/dev/null

echo "Starting PostgreSQL service"
docker compose up -d db

attempt=1
while [ "${attempt}" -le 60 ]; do
    if docker compose exec -T db pg_isready -U postgres -d asset_discovery >/dev/null 2>&1; then
        break
    fi
    if [ "${attempt}" -eq 60 ]; then
        echo "PostgreSQL did not become ready after 120 seconds" >&2
        exit 1
    fi
    sleep 2
    attempt=$((attempt + 1))
done

echo "Resetting demo assets table for deterministic evidence"
docker compose exec -T db psql -U postgres -d asset_discovery \
    -c "drop table if exists assets;"

echo "Running PCAP fixture and writing discovered assets to PostgreSQL"
docker run --rm \
    --network asset-net \
    -v "$PWD/samples:/samples:ro" \
    -e DB_HOST=db \
    -e DB_PORT=5432 \
    -e DB_NAME=asset_discovery \
    -e DB_USER=postgres \
    -e DB_PASSWORD=123456 \
    "${image_name}" \
    --pcap /samples/multi-asset.pcap \
    --filter "${filter_expression}" \
    --output json

echo "Querying PostgreSQL evidence"
docker compose exec -T db psql -U postgres -d asset_discovery \
    -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets order by mac_address;"

echo "Docker runtime verification completed"
