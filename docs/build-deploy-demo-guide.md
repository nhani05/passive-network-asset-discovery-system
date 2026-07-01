# Hướng Dẫn Build, Deploy, Và Demo Chi Tiết

Tài liệu này dùng để demo từ checkout sạch của repository. Tất cả lệnh mặc định chạy ở thư mục gốc repository, trừ khi có ghi chú khác.

Tài liệu bao phủ toàn bộ trường hợp demo có thể thực hiện với hệ thống hiện tại:

| # | Nhóm demo | Mô tả |
|---|-----------|-------|
| 1 | Chuẩn bị trước demo | Kiểm tra công cụ, interface, PostgreSQL |
| 2 | Build và test | Build native có/không có libpcap, chạy CTest |
| 3 | PCAP offline | Table, JSON, CSV, BPF filter, multi-asset, edge case |
| 4 | PostgreSQL local | Ghi database qua `.env`, `--db-url`, và biến PG* |
| 5 | Docker image | Build image, chạy PCAP trong container |
| 6 | Docker Compose | Compose PostgreSQL, pcap-demo, DNS check |
| 7 | Script kiểm chứng Docker | verify-docker-runtime.sh |
| 8 | Live capture native | Timed, infinite, stop condition, Ctrl+C |
| 9 | Live capture Docker | Docker run, Compose profile live |
| 10 | Demo xử lý lỗi | BPF sai, file không tồn tại, argument sai |
| 11 | Demo help text | --help / -h |
| 12 | Evidence cần chụp | Checklist evidence |
| 13 | Troubleshooting | Các lỗi phổ biến và cách xử lý |

---

## 1. Chuẩn Bị Trước Demo

### 1.1. Yêu cầu môi trường

| Công cụ | Yêu cầu | Ghi chú |
|---------|----------|---------|
| CMake | 3.16 trở lên | Build hệ thống C++17/CMake |
| C++ compiler | Hỗ trợ C++17 | GCC 8+, Clang 7+, MSVC 2019+ |
| `libpcap-dev` | Tùy chọn | Bắt buộc nếu demo PCAP/live capture |
| Docker | Tùy chọn | Cần cho demo container |
| Docker Compose | Tùy chọn | Cần cho demo Compose stack |
| `psql` | Tùy chọn | Cần cho query PostgreSQL từ host |
| `pkg-config` | Tùy chọn | CMake dùng để tìm libpcap |

### 1.2. Kiểm tra nhanh công cụ

```sh
cmake --version
g++ --version          # hoặc clang++ --version
docker --version
docker compose version
psql --version
pkg-config --version
```

### 1.3. Kiểm tra interface mạng (cho live capture)

```sh
ip -br link
ip route
```

Trong các ví dụ dưới đây dùng `eth0`. Khi demo trên máy thật, thay bằng interface đang dùng, ví dụ `enp4s0`, `wlan0`, `ens33`.

### 1.4. Lưu ý quan trọng về PostgreSQL

- Binary local tự load file `.env` trong thư mục hiện tại khi khởi động.
- Nếu `.env`, `DATABASE_URL`, hoặc biến `PGHOST`, `PGPORT`, `PGDATABASE`, `PGUSER`, `PGPASSWORD`, `PGSERVICE` đang được cấu hình, mỗi lần chạy discovery sẽ ghi asset vào PostgreSQL.
- Nếu chỉ muốn xem output terminal mà không ghi database, chạy từ thư mục không có `.env` hoặc tạm bỏ/comment các biến PostgreSQL trong `.env`.
- Schema `assets` được tạo tự động bằng `CREATE TABLE IF NOT EXISTS`, nên không cần chạy schema thủ công trước mỗi lần demo.
- Ngoài biến `PG*`, binary cũng nhận alias `DB_HOST`, `DB_PORT`, `DB_NAME`, `DB_USER`, `DB_PASSWORD` (dùng trong Docker Compose) và tự map sang biến `PG*` tương ứng nếu biến `PG*` chưa được set.

### 1.5. Cấu trúc CLI

Hệ thống hỗ trợ ba chế độ capture loại trừ nhau:

```text
asset-discovery --pcap <file> [--filter <bpf>] [--output table|json|csv] [--db-url <url>]
asset-discovery --interface <name> --duration <seconds> [--filter <bpf>] [--capture-backend auto|pcap|af-packet] [--output table|json|csv] [--db-url <url>]
asset-discovery --interface <name> --live [--idle-timeout <seconds>] [--max-assets <count>] [--filter <bpf>] [--capture-backend auto|pcap|af-packet] [--output table|json|csv] [--db-url <url>]
```

Tham số chi tiết:

| Tham số | Ý nghĩa |
|---------|---------|
| `--pcap <file>` | Đọc packet từ file PCAP |
| `--interface <name>` | Capture packet từ interface mạng |
| `--duration <seconds>` | Thời lượng capture (dùng với `--interface`) |
| `--live` | Capture không giới hạn cho đến khi dừng |
| `--idle-timeout <seconds>` | Trong `--live`, dừng khi không có packet phù hợp trong N giây |
| `--max-assets <count>` | Trong `--live`, dừng khi phát hiện đủ N asset |
| `--filter <bpf>` | Lọc packet bằng biểu thức BPF |
| `--capture-backend auto\|pcap\|af-packet` | Chọn backend live capture, mặc định `auto`; chỉ dùng với `--interface` |
| `--output table\|json\|csv` | Định dạng output, mặc định `table` |
| `--db-url <url>` | Ghi asset vào PostgreSQL bằng connection string |
| `-h`, `--help` | Hiển thị help text |

---

## 2. Build Native Và Chạy Test

### 2.1. Build không bắt buộc libpcap (cho máy CI/dev thiếu libpcap)

```sh
cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

Kỳ vọng:

- Build hoàn tất, có thể có warning `libpcap was not found` nhưng không lỗi.
- `ctest` báo `100% tests passed`.
- Binary `./build/asset-discovery` tồn tại nhưng chạy capture sẽ báo `backend is not available`.

### 2.2. Build bắt buộc libpcap (cho máy demo cần đọc PCAP/live capture)

```sh
cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Kỳ vọng:

- Build hoàn tất không lỗi, không warning libpcap.
- `ctest` báo `100% tests passed`.
- Nếu `ASSET_DISCOVERY_REQUIRE_PCAP=ON` lỗi ở bước configure, cài `libpcap-dev` rồi chạy lại (xem mục Troubleshooting).

### 2.3. Benchmark concurrency

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
./build-release/asset-discovery-live-benchmark --pcap path/to/user-traffic.pcap \
  --workers 7 --batch-size 256 \
  --filter "arp or udp port 67 or udp port 68"
```

Tham số positional cũ lần lượt là `<pcap-path> [worker-count] [batch-size] [filter]`. Nếu truyền filter, đặt biểu thức BPF trong dấu nháy.

Benchmark so sánh backend live cần Linux host, interface thật, và quyền capture. Chạy cùng interface, duration, filter, worker count, và batch size cho hai backend để so sánh công bằng:

```sh
sudo ./build-release/asset-discovery-live-benchmark \
  --interface eth0 --duration 30 --backend pcap \
  --workers 7 --batch-size 256 \
  --filter "arp or udp port 67 or udp port 68"

sudo ./build-release/asset-discovery-live-benchmark \
  --interface eth0 --duration 30 --backend af-packet \
  --workers 7 --batch-size 256 \
  --filter "arp or udp port 67 or udp port 68"
```

Backend `af-packet` dùng Linux `AF_PACKET`/`TPACKET_V3` nên cần root hoặc `CAP_NET_RAW`. Nếu dùng `--backend auto`, output sẽ cho biết `backend_selected` thực tế và `backend_fallback_reason` nếu runtime fallback về `pcap`.

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

Với live backend comparison, output có thêm:

```text
benchmark=live-backend-comparison
traffic_source=live-interface
interface=eth0
duration_seconds=30
live_capture_metrics ... backend_requested=... backend_selected=... backend_packets_copied=... backend_kernel_drops=... packet_batches_dropped=...
```

Mục tiêu 1M packet/s là mục tiêu benchmark trong Release build với traffic source, packet mix, filter, backend, OS/kernel, quyền capture, và drop rate được báo cáo. Không diễn giải con số này như bảo đảm tuyệt đối cho mọi NIC, OS, packet mix, hoặc live capture không dùng BPF filter.

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

### 2.4. Kiểm tra số lượng test

```sh
ctest --test-dir build --output-on-failure 2>&1 | tail -5
```

Kỳ vọng output:

```text
100% tests passed, 0 tests failed out of 26
```

---

## 3. Demo PCAP Offline

### 3.1. Table output với fixture ARP đơn giản

```sh
./build/asset-discovery --pcap samples/arp.pcap --output table
```

Kỳ vọng output có một asset:

```text
MAC                IPs                     Hostname          First Seen        Last Seen         Sources
--------------------------------------------------------------------------------------------------------
02:42:ac:11:00:02  192.168.1.10                              1699606784.0      1699606784.0      arp
```

Giải thích:

- `samples/arp.pcap` chứa 1 ARP request từ MAC `02:42:ac:11:00:02`, IP `192.168.1.10`.
- Chỉ có 1 asset vì file chỉ có 1 packet ARP.
- Hostname trống vì ARP không mang hostname.
- `first_seen` = `last_seen` vì chỉ có 1 packet.
- Source là `arp`.

### 3.2. JSON output với fixture ARP đơn giản

```sh
./build/asset-discovery --pcap samples/arp.pcap --output json
```

Kỳ vọng JSON là một mảng asset:

```json
[
  {
    "mac_address": "02:42:ac:11:00:02",
    "ip_addresses": ["192.168.1.10"],
    "first_seen": "1699606784.0",
    "last_seen": "1699606784.0",
    "discovery_sources": ["arp"]
  }
]
```

Lưu ý: field `hostname` bị bỏ qua khi chưa biết hostname (không hiển thị `null` hay chuỗi rỗng trong JSON).

### 3.3. CSV output với fixture ARP đơn giản

```sh
./build/asset-discovery --pcap samples/arp.pcap --output csv
```

Kỳ vọng dòng đầu là header, dòng thứ hai là dữ liệu:

```csv
mac_address,ip_addresses,hostname,first_seen,last_seen,discovery_sources
02:42:ac:11:00:02,192.168.1.10,,1699606784.0,1699606784.0,arp
```

Lưu ý:

- Hostname trống là ký tự rỗng giữa hai dấu phẩy.
- Field có nhiều giá trị dùng `;` bên trong field CSV.
- Escape CSV dùng quy tắc quote chuẩn cho dấu phẩy, dấu quote, và ký tự xuống dòng.

### 3.4. Table output mặc định (không truyền --output)

```sh
./build/asset-discovery --pcap samples/arp.pcap
```

Kỳ vọng: output mặc định là `table`, tương tự mục 3.1.

### 3.5. PCAP nhiều asset (multi-asset fixture)

```sh
./build/asset-discovery --pcap samples/multi-asset.pcap --output table
```

Kỳ vọng thấy 4 asset:

```text
MAC                IPs                     Hostname          First Seen        Last Seen         Sources
--------------------------------------------------------------------------------------------------------
02:42:ac:11:00:01  192.168.1.1                               1699606801.1000   1699606801.1000   arp
02:42:ac:11:00:02  192.168.1.10,192.168.1.11                 1699606800.0      1699606807.7000   arp
02:42:ac:11:00:03  192.168.1.20            laptop-user       1699606802.2000   1699606803.3000   arp,dhcp
02:42:ac:11:00:04  192.168.1.30            camera-01         1699606804.4000   1699606804.4000   dhcp
```

Giải thích từng asset:

| MAC | IP | Hostname | Sources | Ghi chú |
|-----|-----|----------|---------|---------|
| `02:42:ac:11:00:01` | `192.168.1.1` | (trống) | `arp` | Gateway, chỉ thấy qua ARP reply |
| `02:42:ac:11:00:02` | `192.168.1.10`, `192.168.1.11` | (trống) | `arp` | 2 IP vì ARP request từ 2 IP khác nhau cùng MAC; table dùng `,` phân cách |
| `02:42:ac:11:00:03` | `192.168.1.20` | `laptop-user` | `arp,dhcp` | Hostname từ DHCP option 12, xuất hiện cả ARP và DHCP |
| `02:42:ac:11:00:04` | `192.168.1.30` | `camera-01` | `dhcp` | Chỉ xuất hiện trong DHCP, có hostname |

### 3.6. Multi-asset JSON output

```sh
./build/asset-discovery --pcap samples/multi-asset.pcap --output json
```

Kỳ vọng:

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

### 3.7. Multi-asset CSV output

```sh
./build/asset-discovery --pcap samples/multi-asset.pcap --output csv
```

Kỳ vọng:

```csv
mac_address,ip_addresses,hostname,first_seen,last_seen,discovery_sources
02:42:ac:11:00:01,192.168.1.1,,1699606801.1000,1699606801.1000,arp
02:42:ac:11:00:02,192.168.1.10;192.168.1.11,,1699606800.0,1699606807.7000,arp
02:42:ac:11:00:03,192.168.1.20,laptop-user,1699606802.2000,1699606803.3000,arp;dhcp
02:42:ac:11:00:04,192.168.1.30,camera-01,1699606804.4000,1699606804.4000,dhcp
```

### 3.8. PCAP với BPF filter chỉ ARP

```sh
./build/asset-discovery --pcap samples/multi-asset.pcap \
  --filter "arp" \
  --output table
```

Kỳ vọng: chỉ thấy asset từ ARP (không có asset chỉ xuất hiện qua DHCP như `02:42:ac:11:00:04`). Asset `02:42:ac:11:00:03` sẽ chỉ có source `arp` (không có `dhcp`) vì DHCP packet bị filter.

### 3.9. PCAP với BPF filter ARP và DHCP

```sh
./build/asset-discovery --pcap samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Kỳ vọng: thấy cả 4 asset như mục 3.5 vì filter bao gồm cả ARP và DHCP.

### 3.10. So sánh output có và không có filter

Demo không filter (hiển thị tất cả packet bao gồm cả IPv4 UDP không phải DHCP):

```sh
./build/asset-discovery --pcap samples/multi-asset.pcap --output table
```

Demo có filter (chỉ ARP/DHCP):

```sh
./build/asset-discovery --pcap samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Lưu ý: fixture `multi-asset.pcap` có 8 packet gồm ARP, DHCP, IPv4 UDP không phải DHCP, và IPv6. Packet không phải ARP/DHCP/DNS hoặc IPv6 sẽ bị parser bỏ qua an toàn ngay cả khi không dùng BPF filter.

---

## 4. Demo PostgreSQL Local Qua Docker Compose

### 4.1. Khởi động database

```sh
docker compose up -d db
docker compose ps
```

Kỳ vọng: service `db` ở trạng thái `running (healthy)`.

### 4.2. Cấu hình `.env` cho binary local

Tạo hoặc cập nhật `.env` trong thư mục repository:

```env
PGHOST=localhost
PGPORT=5432
PGDATABASE=asset_discovery
PGUSER=postgres
PGPASSWORD=123456
```

Hoặc copy từ `.env.example`:

```sh
cp .env.example .env
```

### 4.3. Chạy discovery và ghi PostgreSQL (qua `.env`)

```sh
./build/asset-discovery --pcap samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output json
```

Binary tự load `.env`, phát hiện biến PG*, và ghi asset vào PostgreSQL trước khi render JSON ra terminal.

### 4.4. Truy vấn evidence bằng `psql`

```sh
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets order by mac_address;"
```

Kỳ vọng:

```text
    mac_address    |        ip_addresses         |  hostname   |   first_seen    |    last_seen    | discovery_sources
-------------------+-----------------------------+-------------+-----------------+-----------------+-------------------
 02:42:ac:11:00:01 | {192.168.1.1}               |             | 1699606801.1000 | 1699606801.1000 | {arp}
 02:42:ac:11:00:02 | {192.168.1.10,192.168.1.11} |             | 1699606800.0    | 1699606807.7000 | {arp}
 02:42:ac:11:00:03 | {192.168.1.20}              | laptop-user | 1699606802.2000 | 1699606803.3000 | {arp,dhcp}
 02:42:ac:11:00:04 | {192.168.1.30}              | camera-01   | 1699606804.4000 | 1699606804.4000 | {dhcp}
(4 rows)
```

### 4.5. Ghi PostgreSQL bằng `--db-url` (thay vì `.env`)

```sh
./build/asset-discovery --pcap samples/arp.pcap \
  --db-url "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  --output json
```

Lưu ý: `--db-url` ưu tiên hơn biến `DATABASE_URL` trong `.env`.

### 4.6. Ghi PostgreSQL bằng biến `DATABASE_URL`

```sh
export DATABASE_URL="postgresql://postgres:123456@localhost:5432/asset_discovery"
./build/asset-discovery --pcap samples/arp.pcap --output json
```

### 4.7. Chạy lại PCAP để demo upsert (không duplicate)

```sh
./build/asset-discovery --pcap samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table

psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select count(*) from assets;"
```

Kỳ vọng: vẫn chỉ có 4 rows vì writer dùng `INSERT ... ON CONFLICT (mac_address) DO UPDATE`.

### 4.8. Reset dữ liệu demo để chạy lại cho deterministic evidence

```sh
docker compose exec -T db psql -U postgres -d asset_discovery \
  -c "drop table if exists assets;"
```

Sau đó chạy lại discovery. Binary sẽ tự tạo lại bảng `assets`.

### 4.9. Chạy output terminal trong khi repository đang có `.env`

Nếu chỉ muốn chạy output terminal mà không ghi PostgreSQL:

**Cách 1**: Chạy từ thư mục không có `.env`:

```sh
repo_dir="$PWD"
cd /tmp
"$repo_dir/build/asset-discovery" \
  --pcap "$repo_dir/samples/arp.pcap" \
  --output table
```

**Cách 2**: Tạm comment biến PostgreSQL trong `.env`.

**Cách 3**: Unset biến PG* trước khi chạy:

```sh
env -u PGHOST -u PGPORT -u PGDATABASE -u PGUSER -u PGPASSWORD \
  ./build/asset-discovery --pcap samples/arp.pcap --output table
```

### 4.10. Demo schema tự tạo

Bắt đầu từ database sạch, không có bảng `assets`:

```sh
docker compose exec -T db psql -U postgres -d asset_discovery \
  -c "drop table if exists assets;"

docker compose exec -T db psql -U postgres -d asset_discovery \
  -c "\dt"
# Kỳ vọng: không có bảng nào

./build/asset-discovery --pcap samples/arp.pcap --output table

docker compose exec -T db psql -U postgres -d asset_discovery \
  -c "\dt"
# Kỳ vọng: bảng assets đã được tạo tự động
```

---

## 5. Demo Docker Image

### 5.1. Build image

```sh
docker build -t passive-asset-discovery .
```

Kỳ vọng:

- Multi-stage build: stage build cài CMake, compiler, libpcap-dev; stage runtime cài libpcap0.8, postgresql-client, iputils-ping.
- Image chạy bằng user `asset` (non-root), entrypoint là `asset-discovery`.

### 5.2. Chạy PCAP ARP đơn giản bằng Docker

```sh
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  passive-asset-discovery \
  --pcap /samples/arp.pcap \
  --output table
```

Kỳ vọng: output tương tự mục 3.1.

### 5.3. Chạy PCAP multi-asset JSON bằng Docker

```sh
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  passive-asset-discovery \
  --pcap /samples/multi-asset.pcap \
  --output json
```

Kỳ vọng: output tương tự mục 3.6.

### 5.4. Chạy PCAP với BPF filter bằng Docker

```sh
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  passive-asset-discovery \
  --pcap /samples/arp.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

### 5.5. Chạy PCAP CSV bằng Docker

```sh
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  passive-asset-discovery \
  --pcap /samples/multi-asset.pcap \
  --output csv
```

### 5.6. Demo help text trong Docker

```sh
docker run --rm passive-asset-discovery --help
```

Kỳ vọng: hiển thị usage text với tất cả options.

### 5.7. Chạy PCAP và ghi PostgreSQL từ Docker container

Container cần kết nối được tới PostgreSQL. Nếu dùng Compose network `asset-net`:

```sh
docker compose up -d db

docker run --rm \
  --network asset-net \
  -v "$PWD/samples:/samples:ro" \
  -e DB_HOST=db \
  -e DB_PORT=5432 \
  -e DB_NAME=asset_discovery \
  -e DB_USER=postgres \
  -e DB_PASSWORD=123456 \
  passive-asset-discovery \
  --pcap /samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output json

docker compose exec -T db psql -U postgres -d asset_discovery \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets order by mac_address;"
```

---

## 6. Demo Docker Compose

### 6.1. Validate Compose file

```sh
docker compose config
```

Kỳ vọng: YAML hợp lệ, hiển thị cấu hình đầy đủ của `db`, `pcap-demo`, và `live-capture`.

### 6.2. Chạy demo PCAP với PostgreSQL

```sh
docker compose up --build pcap-demo
```

Giải thích:

- Compose build image từ `Dockerfile`.
- Service `db` khởi động trước, healthcheck đảm bảo PostgreSQL sẵn sàng.
- Service `pcap-demo` đợi `db` healthy, sau đó chạy `samples/multi-asset.pcap` với filter `arp or udp port 67 or udp port 68`, ghi PostgreSQL, in table output.
- Container `pcap-demo` giữ sống sau khi chạy xong để kiểm tra DNS/DB.

### 6.3. Kiểm tra database từ Compose

```sh
docker compose exec db psql -U postgres -d asset_discovery \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets order by mac_address;"
```

### 6.4. Kiểm tra DNS/network giữa các service

```sh
docker compose exec pcap-demo ping db
```

Kỳ vọng: ping thành công, chứng minh DNS giữa container hoạt động.

### 6.5. Kiểm tra trạng thái service

```sh
docker compose ps
```

Kỳ vọng:

```text
NAME       COMMAND                  SERVICE     STATUS
db         "docker-entrypoint.s…"   db          running (healthy)
pcap-demo  "/bin/sh -c '...'"       pcap-demo   running
```

### 6.6. Kiểm tra schema và dữ liệu chi tiết

```sh
docker compose exec db psql -U postgres -d asset_discovery \
  -c "\d assets"
```

Kỳ vọng: hiển thị schema bảng `assets` gồm `mac_address`, `ip_addresses`, `hostname`, `first_seen`, `last_seen`, `discovery_sources`, `updated_at`.

### 6.7. Dừng demo

```sh
docker compose down
```

### 6.8. Dừng và xóa volume demo PostgreSQL khi muốn reset credential/dữ liệu

```sh
docker compose down -v
```

Lưu ý: `-v` xóa volume `postgres-data`, mất toàn bộ dữ liệu database. Chỉ dùng khi muốn reset hoàn toàn.

### 6.9. Chạy binary local ghi vào PostgreSQL của Compose

```sh
docker compose up -d db
./build/asset-discovery --pcap samples/arp.pcap --output json
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets;"
```

Compose publish PostgreSQL trên port host mặc định `5432`. Nếu máy đã có PostgreSQL local dùng port này:

```sh
POSTGRES_HOST_PORT=5433 docker compose up -d db
# Cập nhật .env: PGPORT=5433
```

---

## 7. Script Kiểm Chứng Docker Runtime

### 7.1. Chạy script

```sh
scripts/verify-docker-runtime.sh
```

### 7.2. Các bước script thực hiện

| Bước | Hành động |
|------|-----------|
| 1 | Build Docker image `passive-asset-discovery` |
| 2 | Chạy `samples/arp.pcap` trong container, in table output |
| 3 | Validate `docker compose config` |
| 4 | Start PostgreSQL service |
| 5 | Đợi PostgreSQL healthy (tối đa 120 giây) |
| 6 | Reset bảng demo `assets` (drop table) |
| 7 | Chạy `samples/multi-asset.pcap` trong container, ghi PostgreSQL, in JSON output |
| 8 | Query evidence từ database |

### 7.3. Customize script

```sh
IMAGE_NAME=my-custom-image CAPTURE_FILTER="arp" scripts/verify-docker-runtime.sh
```

### 7.4. Kỳ vọng output cuối

```text
Docker runtime verification completed
```

Đây là lệnh phù hợp để lấy evidence cuối trước khi bàn giao.

---

## 8. Demo Live Capture Native Linux

### 8.1. Live timed capture (có thời hạn)

Capture trong 60 giây:

```sh
sudo ./build/asset-discovery --interface eth0 --duration 60 \
  --capture-backend auto \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Ví dụ với interface `enp4s0`:

```sh
sudo ./build/asset-discovery --interface enp4s0 --duration 60 \
  --capture-backend auto \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Kỳ vọng:

- Chương trình capture đúng 60 giây rồi render kết quả.
- Nếu có traffic ARP/DHCP trong LAN, sẽ thấy các thiết bị mạng.
- Nếu không có traffic, output là bảng trống hoặc `No assets discovered.`.

Live capture ghi metrics ra stderr sau khi dừng, còn stdout vẫn giữ nguyên format table/JSON/CSV để có thể pipe hoặc validate. Các trường đáng chú ý:

- `packets_captured`: packet đã được callback từ libpcap sau BPF filter.
- `packets_enqueued`: packet đã vào bounded queue để parser workers xử lý.
- `packets_dropped_queue_full`: packet bị drop ở application queue khi burst vượt capacity.
- `packets_parsed`: packet parser workers đã xử lý.
- `observations_applied`: observation aggregator đã merge vào `AssetStore`.
- `packet_throughput_per_second`: throughput của parser pipeline trong phiên capture.
- `backend_requested`: backend người dùng yêu cầu, ví dụ `auto`, `pcap`, hoặc `af-packet`.
- `backend_selected`: backend live capture thực tế được dùng.
- `backend_fallback_reason`: lý do `auto` fallback, nếu có.
- `backend_packets_dropped`: drop counter từ backend nếu hỗ trợ.
- `backend_packets_copied`: số packet backend phải copy trước khi đưa vào pipeline.
- `backend_kernel_drops`: drop counter từ kernel ring với `af-packet` khi kernel cung cấp.
- `packet_batches_dropped`: số batch bị drop ở application queue.

### 8.2. Live timed capture ngắn cho demo nhanh

```sh
sudo ./build/asset-discovery --interface eth0 --duration 10 \
  --capture-backend auto \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

### 8.3. Live timed capture với JSON output

```sh
sudo ./build/asset-discovery --interface eth0 --duration 30 \
  --capture-backend auto \
  --filter "arp or udp port 67 or udp port 68" \
  --output json
```

### 8.4. Live timed capture với CSV output

```sh
sudo ./build/asset-discovery --interface eth0 --duration 30 \
  --capture-backend auto \
  --filter "arp or udp port 67 or udp port 68" \
  --output csv
```

### 8.5. Live timed capture ghi PostgreSQL

```sh
sudo ./build/asset-discovery --interface eth0 --duration 30 \
  --capture-backend auto \
  --db-url "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

### 8.6. Live infinite capture

```sh
sudo ./build/asset-discovery --interface eth0 --live \
  --capture-backend auto \
  --idle-timeout 30 \
  --max-assets 10 \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

### 8.7. Ý nghĩa stop condition trong live mode

| Stop condition | Ý nghĩa |
|---------------|---------|
| `--duration 60` | Capture đúng 60 giây rồi render kết quả |
| `--live` | Capture đến khi nhấn Ctrl+C hoặc gặp stop condition |
| `--idle-timeout 30` | Trong `--live`, dừng khi không có packet được BPF filter chấp nhận trong 30 giây |
| `--max-assets 10` | Trong `--live`, dừng khi đã phát hiện 10 asset |
| Ctrl+C / SIGINT | Dừng graceful bất kỳ lúc nào, chương trình vẫn render kết quả cuối |

Lưu ý:

- Trong live mode, packet bị BPF filter loại ra không reset idle timer.
- Ctrl+C/SIGINT dừng graceful, sau đó chương trình vẫn render kết quả cuối và ghi PostgreSQL nếu môi trường đã cấu hình.
- `--idle-timeout` và `--max-assets` chỉ hợp lệ với `--live`, không dùng được với `--duration`.

### 8.8. Live infinite chỉ với idle timeout

```sh
sudo ./build/asset-discovery --interface eth0 --live \
  --capture-backend auto \
  --idle-timeout 15 \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

### 8.9. Live infinite chỉ với max assets

```sh
sudo ./build/asset-discovery --interface eth0 --live \
  --capture-backend auto \
  --max-assets 5 \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

### 8.10. Live infinite không có stop condition (chỉ dừng bằng Ctrl+C)

```sh
sudo ./build/asset-discovery --interface eth0 --live \
  --capture-backend auto \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Nhấn `Ctrl+C` để dừng. Chương trình sẽ render kết quả đã thu được.

### 8.11. Tạo traffic để demo

Nếu traffic thật không có ARP/DHCP trong thời gian demo, output có thể là:

```text
No assets discovered.
```

Cách tạo traffic dễ quan sát:

```sh
# Terminal 2: ping gateway để phát sinh ARP
ping -c 5 $(ip route | awk '/default/ {print $3}')

# Terminal 2: arping để phát sinh ARP request trực tiếp
sudo arping -c 3 192.168.1.1

# Terminal 2: dhclient để phát sinh DHCP
sudo dhclient -v eth0
```

### 8.12. Live capture không có filter (capture tất cả Ethernet)

```sh
sudo ./build/asset-discovery --interface eth0 --duration 10 \
  --capture-backend auto \
  --output table
```

Lưu ý: không có BPF filter, tất cả packet Ethernet đều vào parser. Parser vẫn bỏ qua packet không phải ARP/DHCP/DNS.

### 8.13. Chọn backend live capture

`--capture-backend pcap` ép dùng libpcap/Npcap portable baseline:

```sh
sudo ./build/asset-discovery --interface eth0 --duration 30 \
  --capture-backend pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

`--capture-backend af-packet` ép dùng Linux `AF_PACKET`/`TPACKET_V3`:

```sh
sudo ./build/asset-discovery --interface eth0 --duration 30 \
  --capture-backend af-packet \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Nếu không chạy bằng root hoặc binary không có `CAP_NET_RAW`, backend `af-packet` sẽ báo lỗi rõ ràng. `--capture-backend auto` phù hợp cho demo portable vì tự fallback về `pcap` khi `af-packet` không khả dụng.

---

## 9. Demo Live Capture Bằng Docker

Docker live capture cần Linux host, `--net=host`, và capability capture.

### 9.1. Docker live timed capture

```sh
docker run --rm --user 0:0 --net=host --cap-add=NET_ADMIN --cap-add=NET_RAW \
  passive-asset-discovery \
  --interface eth0 --duration 60 \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

### 9.2. Docker live infinite capture

```sh
docker run --rm --user 0:0 --net=host --cap-add=NET_ADMIN --cap-add=NET_RAW \
  passive-asset-discovery \
  --interface eth0 --live --idle-timeout 30 --max-assets 10 \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

### 9.3. Giải thích flags Docker cho live capture

| Flag | Ý nghĩa |
|------|---------|
| `--user 0:0` | Chạy container bằng root để có quyền capture |
| `--net=host` | Dùng network stack của host, cho phép truy cập interface thật |
| `--cap-add=NET_ADMIN` | Quyền quản lý network (cần cho promiscuous mode) |
| `--cap-add=NET_RAW` | Quyền raw socket, bắt buộc cho `af-packet` và thường cần cho live capture |

### 9.4. Compose profile live timed

```sh
CAPTURE_INTERFACE=eth0 CAPTURE_DURATION=60 docker compose --profile live run --rm live-capture
```

### 9.5. Compose profile live infinite

```sh
CAPTURE_INTERFACE=eth0 CAPTURE_MODE=live CAPTURE_IDLE_TIMEOUT=30 CAPTURE_MAX_ASSETS=10 \
  docker compose --profile live run --rm live-capture
```

### 9.6. Compose profile với interface tùy chỉnh

```sh
CAPTURE_INTERFACE=enp4s0 CAPTURE_DURATION=30 \
  docker compose --profile live run --rm live-capture
```

### 9.7. Compose profile với filter tùy chỉnh

```sh
CAPTURE_INTERFACE=eth0 CAPTURE_DURATION=30 CAPTURE_FILTER="arp" \
  docker compose --profile live run --rm live-capture
```

### 9.8. Biến môi trường cho Compose live profile

| Biến | Mặc định | Ý nghĩa |
|------|----------|---------|
| `CAPTURE_INTERFACE` | `eth0` | Tên interface capture |
| `CAPTURE_MODE` | `timed` | `timed` hoặc `live` (infinite) |
| `CAPTURE_DURATION` | `60` | Thời lượng capture (chỉ khi `CAPTURE_MODE=timed`) |
| `CAPTURE_IDLE_TIMEOUT` | (trống) | Idle timeout cho live mode |
| `CAPTURE_MAX_ASSETS` | (trống) | Max assets cho live mode |
| `CAPTURE_FILTER` | `arp or udp port 67 or udp port 68` | Biểu thức BPF filter |

### 9.9. Lưu ý Docker Desktop

Docker Desktop trên macOS/Windows không tương đương Linux host networking, nên live capture trong container có thể không dùng được. Lý do:

- Docker Desktop chạy Linux VM, `--net=host` gắn vào VM chứ không phải host thật.
- Interface trong VM không phải interface mạng thật của macOS/Windows.

---

## 10. Demo Xử Lý Lỗi

### 10.1. BPF filter sai cú pháp

```sh
./build/asset-discovery --pcap samples/arp.pcap --filter "invalid" --output table
```

Kỳ vọng exit code ≠ 0 và lỗi:

```text
error: invalid BPF filter for 'samples/arp.pcap': can't parse filter expression: syntax error
```

### 10.2. File PCAP không tồn tại

```sh
./build/asset-discovery --pcap nonexistent.pcap --output table
```

Kỳ vọng exit code ≠ 0 và lỗi chỉ ra file không tìm thấy.

### 10.3. Thiếu nguồn input

```sh
./build/asset-discovery --output table
```

Kỳ vọng exit code ≠ 0 và lỗi:

```text
error: provide exactly one input source: --pcap <file> or --interface <name>
```

### 10.4. Cả hai nguồn input cùng lúc

```sh
./build/asset-discovery --pcap samples/arp.pcap --interface eth0 --duration 10 --output table
```

Kỳ vọng exit code ≠ 0 và lỗi:

```text
error: provide exactly one input source: --pcap <file> or --interface <name>
```

### 10.5. `--interface` thiếu `--duration` hoặc `--live`

```sh
./build/asset-discovery --interface eth0 --output table
```

Kỳ vọng exit code ≠ 0 và lỗi:

```text
error: --interface requires either --duration <seconds> or --live
```

### 10.6. `--duration` và `--live` cùng lúc

```sh
./build/asset-discovery --interface eth0 --duration 60 --live --output table
```

Kỳ vọng exit code ≠ 0 và lỗi:

```text
error: choose either --duration <seconds> or --live with --interface, not both
```

### 10.7. `--idle-timeout` với `--duration` (không hợp lệ)

```sh
./build/asset-discovery --interface eth0 --duration 60 --idle-timeout 30 --output table
```

Kỳ vọng exit code ≠ 0 và lỗi:

```text
error: --idle-timeout is only valid with --interface <name> --live
```

### 10.8. `--max-assets` với `--pcap` (không hợp lệ)

```sh
./build/asset-discovery --pcap samples/arp.pcap --max-assets 5 --output table
```

Kỳ vọng exit code ≠ 0 và lỗi:

```text
error: --max-assets is only valid with --interface <name> --live
```

### 10.9. `--duration` với giá trị không hợp lệ

```sh
./build/asset-discovery --interface eth0 --duration -5 --output table
```

Kỳ vọng exit code ≠ 0 và lỗi:

```text
error: --duration must be a positive integer
```

### 10.10. `--output` với format không hỗ trợ

```sh
./build/asset-discovery --pcap samples/arp.pcap --output xml
```

Kỳ vọng exit code ≠ 0 và lỗi:

```text
error: output format 'xml' is not supported; expected one of: table, json, csv
```

### 10.11. Argument không nhận biết

```sh
./build/asset-discovery --pcap samples/arp.pcap --verbose
```

Kỳ vọng exit code ≠ 0 và lỗi:

```text
error: unknown argument '--verbose'
```

### 10.12. `--filter` rỗng

```sh
./build/asset-discovery --pcap samples/arp.pcap --filter "" --output table
```

Kỳ vọng exit code ≠ 0 và lỗi:

```text
error: --filter cannot be empty
```

### 10.13. `--db-url` rỗng

```sh
./build/asset-discovery --pcap samples/arp.pcap --db-url "" --output table
```

Kỳ vọng exit code ≠ 0 và lỗi:

```text
error: --db-url cannot be empty; use DATABASE_URL in .env or pass a valid connection string
```

### 10.14. PostgreSQL không kết nối được

```sh
./build/asset-discovery --pcap samples/arp.pcap \
  --db-url "postgresql://postgres:wrong@localhost:9999/nonexistent" \
  --output table
```

Kỳ vọng exit code ≠ 0 và lỗi từ `psql` client.

### 10.15. Build không có libpcap, chạy capture

Trên máy không có libpcap, build với `REQUIRE_PCAP=OFF`:

```sh
cmake -S . -B build-no-pcap -DASSET_DISCOVERY_REQUIRE_PCAP=OFF
cmake --build build-no-pcap
./build-no-pcap/asset-discovery --pcap samples/arp.pcap --output table
```

Kỳ vọng: warning `backend ... is not available in this build` và không đọc được PCAP.

---

## 11. Demo Help Text

### 11.1. Hiển thị help bằng `--help`

```sh
./build/asset-discovery --help
```

### 11.2. Hiển thị help bằng `-h`

```sh
./build/asset-discovery -h
```

Kỳ vọng output:

```text
Usage:
  asset-discovery --pcap <file> [--filter <bpf>] [--output table|json|csv] [--db-url <url>]
  asset-discovery --interface <name> --duration <seconds> [--filter <bpf>] [--capture-backend auto|pcap|af-packet] [--output table|json|csv] [--db-url <url>]
  asset-discovery --interface <name> --live [--idle-timeout <seconds>] [--max-assets <count>] [--filter <bpf>] [--capture-backend auto|pcap|af-packet] [--output table|json|csv] [--db-url <url>]

Options:
  --pcap <file>              Read packets from a PCAP file.
  --interface <name>         Capture packets from a live interface.
  --duration <seconds>       Live timed capture duration.
  --live                     Capture until stopped by Ctrl+C or a live stop condition.
  --idle-timeout <seconds>   In --live mode, stop after no accepted packet is seen for this many seconds.
  --max-assets <count>       In --live mode, stop after discovering this many assets.
  --filter <bpf>             Filter packets with a BPF expression, for example: arp or udp port 67 or udp port 68.
  --capture-backend <name>   Live capture backend: auto, pcap, or af-packet. Defaults to auto.
  --output table|json|csv    Output format. Defaults to table.
  --db-url <url>             Write assets to PostgreSQL with the psql client.
  -h, --help                 Show this help text.
```

---

## 12. Evidence Nên Chụp Khi Demo

Checklist evidence theo thứ tự demo:

| # | Evidence | Lệnh |
|---|----------|------|
| 1 | CTest kết quả 100% | `ctest --test-dir build --output-on-failure` |
| 2 | Help text | `./build/asset-discovery --help` |
| 3 | PCAP table output ARP đơn giản | `./build/asset-discovery --pcap samples/arp.pcap --output table` |
| 4 | PCAP JSON output ARP | `./build/asset-discovery --pcap samples/arp.pcap --output json` |
| 5 | PCAP CSV output ARP | `./build/asset-discovery --pcap samples/arp.pcap --output csv` |
| 6 | Multi-asset table output | `./build/asset-discovery --pcap samples/multi-asset.pcap --output table` |
| 7 | Multi-asset JSON output | `./build/asset-discovery --pcap samples/multi-asset.pcap --output json` |
| 8 | Multi-asset với BPF filter | `./build/asset-discovery --pcap samples/multi-asset.pcap --filter "arp or udp port 67 or udp port 68" --output table` |
| 9 | Xử lý lỗi BPF sai | `./build/asset-discovery --pcap samples/arp.pcap --filter "invalid" --output table` |
| 10 | Xử lý lỗi argument sai | `./build/asset-discovery --pcap samples/arp.pcap --output xml` |
| 11 | PostgreSQL query evidence | `psql "postgresql://..." -c "select ... from assets order by mac_address;"` |
| 12 | Docker container PCAP output | `docker run --rm -v ... passive-asset-discovery --pcap /samples/arp.pcap --output table` |
| 13 | Docker Compose service status | `docker compose ps` |
| 14 | Docker runtime verification | `scripts/verify-docker-runtime.sh` |
| 15 | Live capture output (hoặc lỗi quyền) | `sudo ./build/asset-discovery --interface eth0 --duration 10 ...` |
| 16 | Upsert evidence (chạy lại vẫn 4 rows) | `psql "..." -c "select count(*) from assets;"` |

---

## 13. Troubleshooting

### 13.1. Thiếu libpcap khi configure

```text
libpcap was not found. Install the libpcap development package or set ASSET_DISCOVERY_REQUIRE_PCAP=OFF.
```

Cách xử lý:

```sh
sudo apt-get update
sudo apt-get install -y libpcap-dev
cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=ON
cmake --build build
```

### 13.2. Không đủ quyền live capture

- Native Linux: chạy bằng `sudo`.
- Docker: dùng `--user 0:0 --net=host --cap-add=NET_ADMIN --cap-add=NET_RAW`.
- Nếu không muốn dùng sudo, có thể cấp quyền capture cho binary:

```sh
sudo setcap cap_net_raw,cap_net_admin=eip ./build/asset-discovery
```

### 13.3. Database không kết nối được

- Kiểm tra `docker compose ps` — service `db` phải ở trạng thái `running (healthy)`.
- Kiểm tra port host là `5432`, hoặc port trong `POSTGRES_HOST_PORT` nếu đã override.
- Kiểm tra `.env`, `DATABASE_URL`, hoặc biến `PG*`.
- Nếu volume PostgreSQL từng tạo với credential khác và chỉ là dữ liệu demo, chạy `docker compose down -v` rồi tạo lại service `db`.

### 13.4. Thấy chương trình cố ghi PostgreSQL dù chỉ muốn xem table

- Kiểm tra repository có `.env` chứa `PG*` không.
- Chạy từ thư mục không có `.env`, hoặc tạm bỏ/comment cấu hình PostgreSQL trong `.env`.
- Hoặc unset biến: `env -u PGHOST -u PGPORT -u PGDATABASE -u PGUSER -u PGPASSWORD ./build/asset-discovery ...`

### 13.5. Thấy thông báo `relation "assets" already exists, skipping` hoặc `CREATE TABLE`

- Đây là output thành công từ `psql` khi schema đã tồn tại, không phải lỗi discovery.
- Rebuild binary mới nhất để dùng PostgreSQL writer quiet mode.
- Nếu vẫn muốn loại bỏ hoàn toàn thao tác DB, chạy không có `.env`/`DATABASE_URL`/`PG*`.

### 13.6. Không thấy asset khi live capture

- Kiểm tra đúng interface bằng `ip -br link`.
- Bỏ `--filter` tạm thời để xác nhận có packet đi qua interface.
- Tăng `--duration`.
- Tạo traffic ARP/DHCP phù hợp trong cùng LAN (xem mục 8.11).

### 13.7. Port 5432 đã bị chiếm bởi PostgreSQL local

```sh
POSTGRES_HOST_PORT=5433 docker compose up -d db
# Cập nhật .env:
# PGPORT=5433
```

### 13.8. Docker build lỗi vì thiếu internet

Multi-stage build cần tải `ubuntu:22.04` base image và `apt-get install` dependencies. Đảm bảo Docker daemon có quyền truy cập internet.

### 13.9. Docker Compose `pcap-demo` thoát lỗi

Kiểm tra log:

```sh
docker compose logs pcap-demo
```

Nguyên nhân phổ biến:

- PostgreSQL chưa sẵn sàng sau 120 giây — kiểm tra log service `db`.
- File PCAP không tồn tại — kiểm tra volume mount `./samples`.

### 13.10. Live capture trong Docker Desktop (macOS/Windows)

Docker Desktop chạy Linux VM, `--net=host` gắn vào VM chứ không phải host thật. Live capture trong Docker Desktop thường không nhìn thấy traffic thật của host.

Giải pháp: dùng native binary trên host thay vì container cho live capture trên macOS/Windows.

### 13.11. `psql` không có trong PATH

PostgreSQL writer dùng `psql` client. Nếu `psql` không có trong PATH:

```sh
sudo apt-get install -y postgresql-client
```

Trong Docker image, `postgresql-client` đã được cài sẵn.
