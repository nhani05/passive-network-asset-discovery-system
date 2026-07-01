# Checklist Bàn Giao Cuối

Checklist này dùng cho Sprint 4/M4 Delivery.

## Artifact Bắt Buộc

- Source code C++17/CMake: `include/`, `src/`, `tests/`, `CMakeLists.txt`.
- Docker image definition: `Dockerfile`.
- Docker Compose demo: `docker-compose.yml`.
- PostgreSQL schema: `db/schema.sql`.
- Sample PCAP: `samples/arp.pcap`, `samples/multi-asset.pcap`.
- Tài liệu đặc tả: [docs/design-spec/asset-output-contract.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/design-spec/asset-output-contract.md) và [docs/design-spec/asset-events.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/design-spec/asset-events.md).
- Tài liệu thiết kế: [docs/design-spec/system-design.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/design-spec/system-design.md).
- Hướng dẫn build/deploy/demo: [docs/build-deploy-demo-guide.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/build-deploy-demo-guide.md).
- Sprint notes: [docs/sprints/SPRINT2.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/sprints/SPRINT2.md), [docs/sprints/SPRINT3.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/sprints/SPRINT3.md), [docs/sprints/SPRINT4.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/sprints/SPRINT4.md).
- Script kiểm chứng Docker: `scripts/verify-docker-runtime.sh`.

## Lệnh Kiểm Chứng Từ Checkout Sạch

```sh
cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

```sh
./build/asset-discovery --pcap samples/arp.pcap \
  --filter "arp" \
  --output table
```

```sh
./build/asset-discovery --pcap samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output json
```

Nếu host có Docker:

```sh
docker build -t passive-asset-discovery .
docker compose config
scripts/verify-docker-runtime.sh
```

Nếu host có quyền live capture:

```sh
sudo ./build/asset-discovery --interface eth0 --duration 10 \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

## Evidence Cần Chụp Khi Demo

- Terminal output của `ctest`.
- Table output từ `samples/arp.pcap`.
- JSON output từ `samples/multi-asset.pcap`.
- Query PostgreSQL:

```sh
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets order by mac_address;"
```

- Docker output từ `scripts/verify-docker-runtime.sh`; script reset bảng demo `assets` để evidence lặp lại được.
- Live capture output hoặc lỗi quyền capture nếu host không cho phép.

## Known Limitations

- Hệ thống chỉ passive monitoring, không chủ động scan host.
- Metadata DHCP phụ thuộc fixture hoặc traffic thật có DHCP option phù hợp.
- PostgreSQL persistence cần service PostgreSQL reachable và `psql` trong image/local PATH.
- Compose database dùng credential demo, không phải cấu hình production.
- Live capture phụ thuộc quyền hệ thống, interface host, và Docker networking.
- Docker live capture được thiết kế cho Linux host; Docker Desktop có giới hạn khác.

## Release/Tag

Tag demo đề xuất sau khi merge nhánh Sprint 4:

```sh
git tag -a v0.4.0-demo -m "Sprint 4 delivery demo"
git push origin v0.4.0-demo
```

Chỉ tạo tag sau khi checklist trên đã chạy thành công trên commit bàn giao.
