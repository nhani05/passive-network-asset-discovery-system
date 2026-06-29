# Sprint 4: Delivery Và Evidence

Sprint 4 tập trung hoàn thiện bộ bàn giao: Docker runtime, Compose PostgreSQL, tài liệu thiết kế, hướng dẫn build/deploy/demo, script kiểm chứng, evidence, và checklist cuối.

## Phạm Vi Issue

| Issue | Trạng thái | Kết quả |
| --- | --- | --- |
| #17 | Hoàn tất | `Dockerfile` build binary C++17 trong image có libpcap runtime và `postgresql-client`. |
| #18 | Hoàn tất | `docker-compose.yml` có service PostgreSQL, healthcheck, volume, `pcap-demo`, và profile `live-capture`. |
| #19 | Hoàn tất | Thiết kế hệ thống nằm ở `docs/system-design.md`. |
| #20 | Hoàn tất | Hướng dẫn build/deploy/demo nằm ở `docs/build-deploy-demo-guide.md`. |
| #24 | Hoàn tất | `scripts/verify-docker-runtime.sh` kiểm tra image, PCAP mode, Compose config, PostgreSQL mode. |
| #25 | Hoàn tất | Demo flow dùng `samples/arp.pcap` và `samples/multi-asset.pcap`; evidence command nằm trong tài liệu này và checklist. |
| #37 | Hoàn tất | Checklist bàn giao cuối nằm ở `docs/final-submission-checklist.md`. |

## Lệnh Kiểm Chứng Local

```sh
cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

Kết quả kiểm chứng ngày 2026-06-29:

```text
100% tests passed, 0 tests failed out of 19
```

Chạy ARP fixture:

```sh
./build/asset-discovery --pcap samples/arp.pcap --filter "arp" --output table
```

Output mong đợi:

```text
MAC                IPs                     Hostname          First Seen        Last Seen         Sources
--------------------------------------------------------------------------------------------------------
02:42:ac:11:00:02  192.168.1.10                              1699606784.0      1699606784.0      arp
```

Chạy multi-asset fixture:

```sh
./build/asset-discovery --pcap samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output json
```

Output mong đợi:

```json
[
  {
    "mac_address": "02:42:ac:11:00:01",
    "ip_addresses": ["192.168.1.1"],
    "first_seen": "1699606801.1000",
    "last_seen": "1699606801.1000",
    "discovery_sources": ["arp"]
  },
  {
    "mac_address": "02:42:ac:11:00:02",
    "ip_addresses": ["192.168.1.10", "192.168.1.11"],
    "first_seen": "1699606800.0",
    "last_seen": "1699606807.7000",
    "discovery_sources": ["arp"]
  },
  {
    "mac_address": "02:42:ac:11:00:03",
    "ip_addresses": ["192.168.1.20"],
    "hostname": "laptop-user",
    "first_seen": "1699606802.2000",
    "last_seen": "1699606803.3000",
    "discovery_sources": ["arp", "dhcp"]
  },
  {
    "mac_address": "02:42:ac:11:00:04",
    "ip_addresses": ["192.168.1.30"],
    "hostname": "camera-01",
    "first_seen": "1699606804.4000",
    "last_seen": "1699606804.4000",
    "discovery_sources": ["dhcp"]
  }
]
```

## Docker Verification

Build image:

```sh
docker build -t passive-asset-discovery .
```

Chạy PCAP trong container:

```sh
docker run --rm -v "$PWD/samples:/samples:ro" passive-asset-discovery \
  --pcap /samples/arp.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Kiểm chứng runtime đầy đủ:

```sh
scripts/verify-docker-runtime.sh
```

Script này in evidence cho PCAP output và PostgreSQL query.
Để evidence lặp lại được, script reset bảng demo `assets` trong Compose database trước khi ghi lại fixture.

Kết quả kiểm chứng ngày 2026-06-29:

```text
Docker runtime verification completed
```

## PostgreSQL Evidence

Query dùng cho demo:

```sh
docker compose exec -T db psql -U postgres -d asset_discovery \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets order by mac_address;"
```

Kết quả mong đợi có ít nhất:

```text
    mac_address    |        ip_addresses         |  hostname   |   first_seen    |    last_seen    | discovery_sources
-------------------+-----------------------------+-------------+-----------------+-----------------+-------------------
 02:42:ac:11:00:01 | {192.168.1.1}               |             | 1699606801.1000 | 1699606801.1000 | {arp}
 02:42:ac:11:00:02 | {192.168.1.10,192.168.1.11} |             | 1699606800.0    | 1699606807.7000 | {arp}
 02:42:ac:11:00:03 | {192.168.1.20}              | laptop-user | 1699606802.2000 | 1699606803.3000 | {arp,dhcp}
 02:42:ac:11:00:04 | {192.168.1.30}              | camera-01   | 1699606804.4000 | 1699606804.4000 | {dhcp}
```

## Live Capture Evidence

Native:

```sh
sudo ./build/asset-discovery --interface eth0 --duration 10 \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Docker:

```sh
CAPTURE_INTERFACE=eth0 CAPTURE_DURATION=10 docker compose --profile live run --rm live-capture
```

Nếu host không cấp quyền capture hoặc không có traffic ARP/DHCP trong thời gian demo, evidence hợp lệ là lỗi quyền/interface hoặc bảng không có asset, kèm ghi chú môi trường.

## Ghi Chú Bàn Giao

- Tài liệu chi tiết: `docs/build-deploy-demo-guide.md`.
- Thiết kế: `docs/system-design.md`.
- Checklist cuối: `docs/final-submission-checklist.md`.
- PostgreSQL Compose publish port host `15432`.
- `.env.example` dùng `PGPORT=15432` để binary local kết nối đúng database Compose.
