# Sprint 4: Delivery Và Evidence

Sprint 4 tập trung hoàn thiện bộ bàn giao: Docker runtime, Compose SQLite, tài liệu thiết kế, hướng dẫn build/deploy/demo, script kiểm chứng, evidence, và checklist cuối.

## Phạm Vi Issue

| Issue | Trạng thái | Kết quả |
| --- | --- | --- |
| #17 | Hoàn tất | `Dockerfile` build binary C++17 trong image có libpcap và SQLite runtime. |
| #18 | Hoàn tất | `docker-compose.yml` có `pcap-demo`, `assetd`, và SQLite volumes. |
| #19 | Hoàn tất | Thiết kế hệ thống nằm ở `docs/system-design.md`. |
| #20 | Hoàn tất | Hướng dẫn build/deploy/demo nằm ở `docs/build-deploy-demo-guide.md`. |
| #24 | Hoàn tất | `scripts/verify-docker-runtime.sh` kiểm tra image, PCAP mode, Compose config, và SQLite output. |
| #25 | Hoàn tất | Demo flow dùng `samples/arp.pcap` và `samples/multi-asset.pcap`; evidence command nằm trong tài liệu này và checklist. |
| #37 | Hoàn tất | Checklist bàn giao cuối nằm ở `docs/final-submission-checklist.md`. |

## Lệnh Kiểm Chứng Local

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Kết quả kiểm chứng ngày 2026-07-01:

```text
100% tests passed, 0 tests failed out of 26
```

Chạy ARP fixture:

```sh
./build/asset-discovery --pcap samples/arp.pcap --sqlite pnad.db --filter "arp" --output table
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
  --sqlite pnad.db \
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
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  -v "$PWD/.docker-data:/data" \
  -e SQLITE_DATABASE_PATH=/data/pnad.db \
  passive-asset-discovery \
  --pcap /samples/arp.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Kiểm chứng runtime đầy đủ:

```sh
scripts/verify-docker-runtime.sh
```

Script này in evidence cho PCAP output và kiểm tra SQLite DB được tạo.
Để evidence lặp lại được, script dùng thư mục SQLite tạm và Compose volume riêng.

Kết quả kiểm chứng ngày 2026-07-01:

```text
Docker runtime verification completed
```

## SQLite Evidence

Query dùng cho demo:

```sh
sqlite3 pnad.db \
  "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets order by mac_address;"
```

Kết quả mong đợi có ít nhất:

```text
02:42:ac:11:00:01|["192.168.1.1"]||1699606801.1000|1699606801.1000|["arp"]
02:42:ac:11:00:02|["192.168.1.10","192.168.1.11"]||1699606800.0|1699606807.7000|["arp"]
02:42:ac:11:00:03|["192.168.1.20"]|laptop-user|1699606802.2000|1699606803.3000|["arp","dhcp"]
02:42:ac:11:00:04|["192.168.1.30"]|camera-01|1699606804.4000|1699606804.4000|["dhcp"]
```

## Removed Live CLI Evidence

CLI chính không còn nhận live capture:

```sh
./build/asset-discovery --interface eth0
```

Kỳ vọng:

```text
[CONFIG ERROR] --interface has been removed; use --pcap <file>
```

## Ghi Chú Bàn Giao

- Tài liệu chi tiết: `docs/build-deploy-demo-guide.md`.
- Thiết kế: `docs/system-design.md`.
- Checklist cuối: `docs/final-submission-checklist.md`.
- Compose dùng SQLite volumes cho `pcap-demo` và `assetd`.
- `.env.example` dùng `SQLITE_DATABASE_PATH=pnad.db` cho runtime local.
