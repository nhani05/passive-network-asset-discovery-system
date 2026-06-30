# Hướng Dẫn Build, Deploy, Và Demo

Tài liệu này dùng cho checkout sạch của repository.

## Yêu Cầu

- CMake 3.16 trở lên.
- Compiler hỗ trợ C++17.
- `libpcap-dev` trên Linux hoặc Npcap SDK trên Windows nếu cần đọc PCAP/live capture.
- Docker và Docker Compose nếu chạy container.
- `psql` nếu chạy PostgreSQL mode bằng binary local.

## Build Native

Môi trường không bắt buộc có libpcap:

```sh
cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

Môi trường demo cần fail fast nếu thiếu libpcap:

```sh
cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=ON
cmake --build build
```

Release build dùng khi đo throughput:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DASSET_DISCOVERY_REQUIRE_PCAP=ON
cmake --build build-release
```

## Benchmark Concurrency

Benchmark offline không cần quyền live capture. Packet input phải lấy từ PCAP do người dùng cung cấp, không sinh cố định trong code:

```sh
./build-release/asset-discovery-live-benchmark path/to/user-traffic.pcap
./build-release/asset-discovery-live-benchmark path/to/user-traffic.pcap 7 256 "arp or udp port 67 or udp port 68"
```

Tham số lần lượt là `<pcap-path> [worker-count] [batch-size] [filter]`. Nếu truyền filter, đặt biểu thức BPF trong dấu nháy.

Output gồm điều kiện benchmark và metrics:

```text
benchmark=live-pipeline-offline-stress
target_packet_rate_per_second=1000000
traffic_source=user-provided-pcap
pcap_path=path/to/user-traffic.pcap
packet_count=...
filter=...
build_type=Release recommended
live_capture_metrics elapsed_seconds=... packets_captured=... packets_enqueued=... packets_dropped_queue_full=... packets_parsed=... packet_throughput_per_second=...
```

Mục tiêu 1M packet/s là mục tiêu benchmark trong Release build với PCAP input do người dùng cung cấp và drop rate được báo cáo. Không diễn giải con số này như bảo đảm tuyệt đối cho mọi NIC, OS, packet mix, hoặc live capture không dùng BPF filter.

Smoke evidence local ngày 2026-06-30 trên Release build tạm thời, PCAP `samples/multi-asset.pcap`, filter `arp or udp port 67 or udp port 68`, 7 parser workers, batch size 256:

```text
traffic_source=user-provided-pcap
pcap_path=samples/multi-asset.pcap
packet_count=6
packets_captured=6
packets_enqueued=6
packets_dropped_queue_full=0
packets_parsed=6
packet_throughput_per_second=12368.61
observations_applied=6
```

Kết quả này chỉ xác nhận benchmark đọc packet từ PCAP được truyền vào và chạy qua concurrent pipeline không drop ở application queue trong fixture nhỏ. Để chứng minh mục tiêu 1M packet/s, cần chạy cùng command với PCAP lớn do người dùng cung cấp, Release build, và ghi lại throughput/drop rate từ output thực tế.

## Chạy PCAP Sample

Table output:

```sh
./build/asset-discovery --pcap samples/arp.pcap --output table
```

JSON output:

```sh
./build/asset-discovery --pcap samples/arp.pcap --output json
```

CSV output:

```sh
./build/asset-discovery --pcap samples/arp.pcap --output csv
```

Chỉ xử lý ARP/DHCP bằng BPF:

```sh
./build/asset-discovery --pcap samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

## PostgreSQL Local Qua Docker Compose

Start database:

```sh
docker compose up -d db
```

Tạo `.env` cho binary local:

```env
PGHOST=localhost
PGPORT=5432
PGDATABASE=asset_discovery
PGUSER=postgres
PGPASSWORD=123456
```

Chạy discovery và ghi database:

```sh
./build/asset-discovery --pcap samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output json
```

Truy vấn bằng `psql`:

```sh
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets order by mac_address;"
```

Nếu muốn truyền connection string trực tiếp:

```sh
./build/asset-discovery --pcap samples/arp.pcap \
  --db-url "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  --output json
```

## Docker Image

Build image:

```sh
docker build -t passive-asset-discovery .
```

Chạy PCAP bằng Docker:

```sh
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  passive-asset-discovery \
  --pcap /samples/arp.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

## Docker Compose Demo

Kiểm tra Compose file:

```sh
docker compose config
```

Chạy demo PCAP với PostgreSQL:

```sh
docker compose up --build pcap-demo
```

Service `pcap-demo` giữ container sống sau khi xử lý PCAP để có thể kiểm tra database:

```sh
docker compose exec db psql -U postgres -d asset_discovery \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets order by mac_address;"
```

Script kiểm chứng Docker runtime:

```sh
scripts/verify-docker-runtime.sh
```

Script này build image, chạy PCAP fixture trong container, validate `docker compose config`, start PostgreSQL, reset bảng demo `assets`, chạy container ghi PostgreSQL, và in query evidence.

## Live Capture

Native Linux:

```sh
sudo ./build/asset-discovery --interface eth0 --duration 60 \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Live capture ghi metrics ra stderr sau khi dừng, còn stdout vẫn giữ nguyên format table/JSON/CSV để có thể pipe hoặc validate. Các trường đáng chú ý:

- `packets_captured`: packet đã được callback từ libpcap sau BPF filter.
- `packets_enqueued`: packet đã vào bounded queue để parser workers xử lý.
- `packets_dropped_queue_full`: packet bị drop ở application queue khi burst vượt capacity.
- `packets_parsed`: packet parser workers đã xử lý.
- `observations_applied`: observation aggregator đã merge vào `AssetStore`.
- `packet_throughput_per_second`: throughput của parser pipeline trong phiên capture.
- `backend_packets_dropped`: drop counter từ libpcap nếu backend hỗ trợ.

Docker trên Linux host:

```sh
docker run --rm --user 0:0 --net=host --cap-add=NET_ADMIN --cap-add=NET_RAW \
  passive-asset-discovery \
  --interface eth0 --duration 60 \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Compose profile:

```sh
CAPTURE_INTERFACE=eth0 CAPTURE_DURATION=60 docker compose --profile live run --rm live-capture
```

Đổi `eth0` theo interface thật của host. Docker Desktop trên macOS/Windows không tương đương Linux host networking, nên live capture trong container có thể không dùng được.

## Troubleshooting

Thiếu libpcap khi configure:

```text
libpcap was not found. Install the libpcap development package or set ASSET_DISCOVERY_REQUIRE_PCAP=OFF.
```

Không đủ quyền live capture: chạy bằng `sudo` trên native Linux hoặc cấp `NET_ADMIN`, `NET_RAW`, `--net=host` cho container.

Database không kết nối được:

- Kiểm tra `docker compose ps`.
- Kiểm tra port host là `5432` nếu dùng Compose, hoặc port trong `POSTGRES_HOST_PORT` nếu đã override.
- Kiểm tra `.env`, `DATABASE_URL`, hoặc biến `PG*`.
- Nếu volume PostgreSQL từng tạo với credential khác và chỉ là dữ liệu demo, chạy `docker compose down -v` rồi tạo lại.
