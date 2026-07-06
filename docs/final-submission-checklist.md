# Checklist Bàn Giao Cuối

Checklist này dùng cho Sprint 4/M4 Delivery.

## Artifact Bắt Buộc

- Source code C++17/CMake: `include/`, `src/`, `tests/`, `CMakeLists.txt`.
- Docker image definition: `Dockerfile`.
- Docker Compose demo: `docker-compose.yml`.
- SQLite schema/migrations: `src/storage/SQLiteWriter.cpp`.
- Sample PCAP: `samples/arp.pcap`, `samples/multi-asset.pcap`.
- Tài liệu đặc tả: [docs/design-spec/asset-output-contract.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/design-spec/asset-output-contract.md) và [docs/design-spec/asset-events.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/design-spec/asset-events.md).
- Tài liệu thiết kế: [docs/design-spec/system-design.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/design-spec/system-design.md).
- Hướng dẫn build/deploy/demo: [docs/build-deploy-demo-guide.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/build-deploy-demo-guide.md).
- Sprint notes: [docs/sprints/SPRINT2.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/sprints/SPRINT2.md), [docs/sprints/SPRINT3.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/sprints/SPRINT3.md), [docs/sprints/SPRINT4.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/sprints/SPRINT4.md).
- Script kiểm chứng Docker: `scripts/verify-docker-runtime.sh`.

## Lệnh Kiểm Chứng Từ Checkout Sạch

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

```sh
./build/asset-discovery --pcap samples/arp.pcap \
  --sqlite pnad.db \
  --filter "arp" \
  --output table
```

```sh
./build/asset-discovery --pcap samples/multi-asset.pcap \
  --sqlite pnad.db \
  --filter "arp or udp port 67 or udp port 68 or udp port 1900 or udp port 5353" \
  --output json
```

Nếu host có Docker:

```sh
docker build -t passive-asset-discovery .
docker compose config
scripts/verify-docker-runtime.sh
```

Live capture không còn trong scope demo; dùng các kịch bản PCAP ở trên.

## Evidence Cần Chụp Khi Demo

- Terminal output của `ctest`.
- Table output từ `samples/arp.pcap`.
- JSON output từ `samples/multi-asset.pcap`.
- Query SQLite:

```sh
sqlite3 pnad.db \
  "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets order by mac_address;"
```

- Docker output từ `scripts/verify-docker-runtime.sh`; script tạo SQLite DB tạm để evidence lặp lại được.
- Bằng chứng không có bảng event log: `sqlite3 pnad.db ".tables"` không hiển thị `asset_events`.

## Known Limitations

- Hệ thống chỉ passive monitoring, không chủ động scan host.
- Metadata DHCP phụ thuộc fixture hoặc traffic thật có DHCP option phù hợp.
- CLI chính chỉ hỗ trợ PCAP offline.
- SQLite path phải writable khi chạy native hoặc trong Docker volume.

## Release/Tag

Tag demo đề xuất sau khi merge nhánh Sprint 4:

```sh
git tag -a v0.4.0-demo -m "Sprint 4 delivery demo"
git push origin v0.4.0-demo
```

Chỉ tạo tag sau khi checklist trên đã chạy thành công trên commit bàn giao.
