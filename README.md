# Passive Network Asset Discovery System

Hệ thống giám sát mạng thụ động viết bằng C++17, dùng để đọc traffic từ file PCAP hoặc network interface, trích xuất observation từ các protocol phổ biến trong LAN, gom các observation thành danh sách asset, ghi snapshot/event vào PostgreSQL và xuất kết quả ra table, JSON hoặc CSV.

Dự án tập trung vào passive monitoring: chương trình không scan chủ động, không gửi packet thăm dò, chỉ phân tích traffic quan sát được.

## Tính năng chính

- Đọc packet từ file PCAP offline hoặc capture live từ interface.
- Hỗ trợ BPF filter, ví dụ `arp or udp port 67 or udp port 68`.
- Parser plugin built-in: ARP, DHCP, DNS, mDNS, LLMNR, TCP SYN-ACK, SSDP và NetBIOS Name Service.
- Gộp asset theo MAC address đã chuẩn hóa chữ thường.
- Lưu IP đã thấy, hostname DHCP option 12, first seen, last seen, source protocol, metadata quan sát được, metadata tham chiếu và derived hints.
- Phát hiện sự kiện kiểu arpwatch: asset mới, đổi IP/MAC, flip-flop IP/MAC, hostname learned/changed, asset reappeared, ARP/Ethernet MAC mismatch và source IP ngoài local subnet.
- Ghi event ra stdout, file NDJSON, syslog và bảng PostgreSQL `asset_events`.
- Ghi snapshot asset cuối cùng vào bảng PostgreSQL `assets`.
- Đóng gói bằng Dockerfile multi-stage và Docker Compose kèm PostgreSQL.

## Trạng thái runtime quan trọng

Phiên bản hiện tại yêu cầu cấu hình PostgreSQL trước khi chạy capture. Nếu thiếu `DATABASE_URL` hoặc biến `PG*`/`DB_*`, chương trình sẽ dừng với lỗi cấu hình:

```text
[CONFIG ERROR] PostgreSQL configuration is required; set DATABASE_URL or PG*/DB_* values in .env or the process environment
```

Writer PostgreSQL gọi client `psql`, vì vậy khi chạy native cần có `psql` trong `PATH`. Khi chạy bằng Docker image của dự án, `psql` đã được cài trong runtime image.

## Kiến trúc

```text
PCAP file / live interface
        |
        v
PacketCaptureBackend
        |
        v
PacketContextBuilder
        |
        v
ParserEngine + built-in parser plugins
        |
        v
AssetObservation
        |
        v
AssetMonitor
        |
        +--> AssetEventDetector -> EventDispatcher
        |                            +--> stdout
        |                            +--> logs/events.ndjson
        |                            +--> syslog
        |                            +--> PostgreSQL asset_events
        |
        v
AssetStore
        |
        +--> table / JSON / CSV summary
        +--> PostgreSQL assets
```

Module chính:

| Khu vực | Đường dẫn | Vai trò |
| --- | --- | --- |
| CLI/config | `src/cli`, `src/config`, `include/pnad/cli`, `include/pnad/config` | Parse tham số, load config/profile/env và validate runtime. |
| Capture | `src/capture`, `include/pnad/capture` | Đọc PCAP hoặc live capture qua `pcap`/`af-packet`. |
| Parser | `src/packet`, `include/pnad/packet` | Decode Ethernet/IPv4/UDP/TCP và chạy parser plugin. |
| Discovery | `src/discovery`, `include/pnad/discovery` | Gộp observation thành asset state và render output. |
| Event | `src/event`, `include/pnad/event` | Phát hiện sự kiện và ghi ra các event sink. |
| Live pipeline | `src/app`, `include/pnad/app` | Pipeline live capture producer/consumer có bounded queue và metrics. |
| Storage | `src/storage`, `include/pnad/storage` | Ghi `assets` và `asset_events` vào PostgreSQL qua `psql`. |

## Yêu cầu môi trường

Native build:

- CMake >= 3.16.
- Compiler C++17, ví dụ GCC hoặc Clang.
- `pkg-config`.
- `libpcap` development package để đọc PCAP/live capture.
- PostgreSQL client `psql`.
- PostgreSQL server reachable qua `DATABASE_URL`, `PG*` hoặc `DB_*`.

Ubuntu/Debian:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config libpcap-dev postgresql-client
```

Docker:

- Docker Engine.
- Docker Compose plugin.
- Live capture trong container cần Linux host, interface thật, `--net=host`, `NET_ADMIN` và `NET_RAW`.

## Quick Start bằng Docker Compose

Cách này không yêu cầu cài compiler hoặc `psql` trên máy host.

```sh
docker compose up --build -d pcap-demo
docker compose logs --no-log-prefix pcap-demo
```

Service `pcap-demo` sẽ:

- build Docker image,
- khởi động PostgreSQL,
- đợi database healthy,
- chạy `asset-discovery --profile pcap --pcap /samples/multi-asset.pcap --output table`,
- giữ container sống để có thể kiểm tra tiếp bằng `docker compose exec`.

Kiểm tra dữ liệu đã ghi vào PostgreSQL:

```sh
docker compose exec db psql -U postgres -d asset_discovery \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets order by mac_address;"

docker compose exec db psql -U postgres -d asset_discovery \
  -c "select event_time, event_type, severity, ip_address, mac_address, protocol from asset_events order by id;"
```

Dọn môi trường demo:

```sh
docker compose down
```

Nếu muốn xóa cả dữ liệu database demo:

```sh
docker compose down -v
```

## Build và test native

Khởi động PostgreSQL bằng Compose với credential mặc định:

```sh
docker compose up -d db
```

Tạo `.env` cho binary native. File `.env` được chương trình tự load từ thư mục hiện tại:

```env
PGHOST=localhost
PGPORT=5432
PGDATABASE=asset_discovery
PGUSER=postgres
PGPASSWORD=123456
PGSSLMODE=disable
```

Build và chạy test:

```sh
cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Nếu chỉ muốn cấu hình project trên môi trường chưa có `libpcap-dev`, có thể bỏ yêu cầu hard fail:

```sh
cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=OFF
```

Chế độ này vẫn build CLI shell, nhưng đọc PCAP/live capture chỉ hoạt động khi binary được build với backend capture khả dụng.

## Chạy PCAP offline

Table output:

```sh
./build/asset-discovery --profile pcap --pcap samples/arp.pcap --output table
```

JSON output:

```sh
./build/asset-discovery --profile pcap --pcap samples/multi-asset.pcap --output json
```

CSV output:

```sh
./build/asset-discovery --profile pcap --pcap samples/multi-asset.pcap --output csv
```

Override BPF filter:

```sh
./build/asset-discovery \
  --pcap samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Lưu ý: event console sink hiện ghi event ra stdout trước phần summary cuối. Vì vậy stdout của `--output json` hoặc `--output csv` có thể không phải JSON/CSV thuần nếu có event được phát hiện. Event dạng machine-readable luôn được append riêng vào `logs/events.ndjson` hoặc path trong `ASSET_DISCOVERY_EVENTS_JSON`.

## Chạy live capture

Native live capture:

```sh
sudo ./build/asset-discovery --profile live --interface eth0
```

Chọn backend:

```sh
sudo ./build/asset-discovery \
  --interface eth0 \
  --capture-backend auto \
  --filter "arp or udp port 67 or udp port 68"
```

Backend hỗ trợ:

- `auto`: tự chọn backend phù hợp.
- `pcap`: dùng libpcap.
- `af-packet`: backend Linux raw socket/packet ring, cần quyền root hoặc capability phù hợp.

Live capture chạy cho tới khi nhận `SIGINT`/`SIGTERM`, ví dụ nhấn `Ctrl+C`. Sau khi dừng, pipeline flush queue/event sink, ghi snapshot cuối vào PostgreSQL và in summary.

Live capture bằng Docker:

```sh
docker build -t passive-asset-discovery .

docker run --rm --user 0:0 --net=host \
  --cap-add=NET_ADMIN --cap-add=NET_RAW \
  --env PGHOST=localhost \
  --env PGPORT=5432 \
  --env PGDATABASE=asset_discovery \
  --env PGUSER=postgres \
  --env PGPASSWORD=123456 \
  passive-asset-discovery \
  --profile live --interface eth0
```

Hoặc dùng Compose profile:

```sh
CAPTURE_INTERFACE=eth0 docker compose --profile live run --rm live-capture
```

## CLI

```text
Usage:
  asset-discovery --pcap <file> [--config <file>|--profile <name>] [--filter <bpf>] [--output table|json|csv]
  asset-discovery --interface <name> [--config <file>|--profile <name>] [--filter <bpf>] [--capture-backend auto|pcap|af-packet]
  asset-discovery --version
```

Tham số chính:

| Tham số | Ý nghĩa |
| --- | --- |
| `--pcap <file>` | Đọc packet từ PCAP file. |
| `--interface <name>` | Capture live từ interface cho tới khi bị dừng. |
| `--config <file>` | Load policy/runtime defaults từ file config. Không đi cùng `--profile`. |
| `--profile <name>` | Load `configs/<name>.yaml`. Không đi cùng `--config`. |
| `--filter <bpf>` | BPF expression áp dụng cho packet capture/read. |
| `--capture-backend auto|pcap|af-packet` | Chỉ hợp lệ với live capture. |
| `--output table|json|csv` | Format summary cuối, mặc định là `json`. |
| `--event-rate-limit <sec>` | Suppress event trùng trong cửa sổ thời gian này. |
| `--event-queue-capacity <n>` | Capacity queue event trong live pipeline, mặc định `1024`. |
| `--flip-flop-window <sec>` | Cửa sổ phát hiện IP/MAC flip-flop. |
| `--reappearance-threshold <sec>` | Ngưỡng inactive trước khi emit `asset_reappeared`. |
| `--local-net <cidr>` | Local IPv4 CIDR cho kiểm tra `non_local_source_ip`; lặp lại được. |
| `--ignore-net <cidr>` | IPv4 CIDR bỏ qua khi kiểm tra non-local; lặp lại được. |
| `--version` | In version binary. |
| `-h`, `--help` | In help text. |

Các cờ cũ như `--duration`, `--live`, `--idle-timeout`, `--max-assets`, `--db-url`, `--events`, `--events-json`, `--syslog` và `--events-db` đã bị loại bỏ. Chạy live hiện mặc định không giới hạn thời gian và dừng bằng signal; database/event sink cấu hình bằng environment.

Exit code:

| Code | Ý nghĩa |
| --- | --- |
| `0` | Thành công. |
| `1` | Lỗi fatal không phân loại. |
| `2` | Lỗi cấu hình hoặc tham số CLI. |
| `3` | Lỗi PCAP/capture. |
| `4` | Lỗi PostgreSQL/database. |

## Cấu hình

Thứ tự merge cấu hình:

```text
built-in defaults
  -> configs/default.yaml nếu tồn tại
  -> --config <file> hoặc --profile <name>
  -> .env/process environment
  -> CLI overrides
```

Config YAML chỉ hỗ trợ các section/key sau:

```yaml
capture:
  filter: "arp or udp port 67 or udp port 68"
  backend: auto

output:
  format: json

events:
  rate_limit_sec: 60
  queue_capacity: 1024
  flip_flop_window_sec: 300
  reappearance_threshold_sec: 15552000

network:
  local_nets: []
  ignore_nets:
    - "127.0.0.0/8"
    - "169.254.0.0/16"
```

Quy tắc config:

- Source packet không được đặt trong YAML. Luôn truyền rõ `--pcap <file>` hoặc `--interface <name>`.
- Database không được đặt trong YAML. Dùng `.env`, `DATABASE_URL`, `PG*` hoặc `DB_*`.
- `--config` và `--profile` không dùng cùng lúc.
- `--profile pcap` tương ứng `configs/pcap.yaml`.
- `--profile live` tương ứng `configs/live.yaml`.
- Parser YAML hiện là parser tối giản của dự án, yêu cầu indent bằng 2 space và không hỗ trợ tab.

Biến môi trường database:

| Biến | Ý nghĩa |
| --- | --- |
| `DATABASE_URL` | Connection string PostgreSQL truyền trực tiếp cho `psql`. |
| `PGHOST`, `PGPORT`, `PGDATABASE`, `PGUSER`, `PGPASSWORD`, `PGSERVICE`, `PGSSLMODE` | Biến chuẩn của `psql`. |
| `DB_HOST`, `DB_PORT`, `DB_NAME`, `DB_USER`, `DB_PASSWORD` | Alias được map sang biến `PG*` nếu biến `PG*` tương ứng chưa có. |
| `ASSET_DISCOVERY_EVENTS_JSON` | Path file event NDJSON, mặc định `logs/events.ndjson`. |

## Output asset

Asset được định danh bằng `mac_address`. Một asset có thể có nhiều IP.

| Field | Format | Ghi chú |
| --- | --- | --- |
| `mac_address` | string | MAC chữ thường, khóa chính trong bảng `assets`. |
| `ip_addresses` | array/list | Tập IPv4 đã quan sát. |
| `hostname` | string/null | Lấy từ DHCP option 12 khi có. |
| `first_seen` | `seconds.microseconds` | Timestamp packet sớm nhất. |
| `last_seen` | `seconds.microseconds` | Timestamp packet mới nhất. |
| `discovery_sources` | array/list | Ví dụ `arp`, `dhcp`, `dns`, `mdns`, `llmnr`, `tcp`, `ssdp`, `netbios`. |
| `observed_metadata` | JSON object | Metadata trích từ packet, ví dụ DHCP option, DNS query, SSDP header. |
| `reference_metadata` | JSON object | Metadata tham chiếu nội bộ, ví dụ thông tin MAC/OUI tối thiểu. |
| `derived_hints` | JSON array | Hint suy luận như vendor, OS, role hoặc device type khi có bằng chứng. |

`table` và `csv` chỉ in snapshot gọn: MAC, IPs, hostname, first/last seen và discovery sources. `json` in thêm metadata/hints.

## Event logging

Event được bật mặc định.

Event types:

| Type | Severity thường gặp | Ý nghĩa |
| --- | --- | --- |
| `new_asset` | `info` | MAC mới xuất hiện lần đầu. |
| `mac_changed_for_ip` | `warning` | IP được quan sát với MAC khác trước đó. |
| `ip_changed_for_mac` | `info` | MAC được quan sát với IP khác trước đó. |
| `ip_mac_flip_flop` | `high` | IP quay lại MAC cũ trong `flip_flop_window_sec`. |
| `asset_reappeared` | `info` | Cặp IP/MAC xuất hiện lại sau `reappearance_threshold_sec`. |
| `hostname_learned` | `info` | Học được hostname mới cho MAC. |
| `hostname_changed` | `warning` | Hostname của MAC thay đổi. |
| `ethernet_arp_mac_mismatch` | `high` | Ethernet source MAC khác ARP sender MAC. |
| `non_local_source_ip` | `warning` | IP quan sát được nằm ngoài `network.local_nets` và không thuộc `ignore_nets`. |

Event sinks:

- stdout: dòng text dễ đọc.
- NDJSON: mặc định append vào `logs/events.ndjson`.
- syslog: một event sink mặc định của hệ thống.
- PostgreSQL: bảng `asset_events`.

## PostgreSQL

Schema nằm trong [db/schema.sql](db/schema.sql).

Các bảng chính:

- `assets`: snapshot cuối theo `mac_address`, gồm IPs, hostname, timestamp, discovery sources, metadata và hints.
- `asset_events`: timeline event, gồm timestamp, type, severity, IP/MAC cũ mới, hostname, protocol, interface, message và metadata.

Writer tự chạy `CREATE TABLE IF NOT EXISTS` trước khi insert, nên không bắt buộc nạp schema thủ công. Có thể nạp schema trước nếu muốn:

```sh
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" -f db/schema.sql
```

Query nhanh:

```sh
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select mac_address, ip_addresses, hostname, discovery_sources from assets order by mac_address;"

psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select event_time, event_type, severity, ip_address, mac_address, protocol from asset_events order by id;"
```

## Docker

Build image:

```sh
docker build -t passive-asset-discovery .
```

Chạy PCAP bằng Docker với PostgreSQL host/Compose đang lắng nghe ở `localhost:5432`:

```sh
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  --env PGHOST=host.docker.internal \
  --env PGPORT=5432 \
  --env PGDATABASE=asset_discovery \
  --env PGUSER=postgres \
  --env PGPASSWORD=123456 \
  passive-asset-discovery \
  --profile pcap --pcap /samples/arp.pcap --output table
```

Trên Linux, nếu `host.docker.internal` chưa được Docker cấu hình, dùng thêm:

```sh
--add-host=host.docker.internal:host-gateway
```

Chạy PCAP bằng Compose:

```sh
docker compose up --build -d pcap-demo
docker compose logs --no-log-prefix pcap-demo
```

Compose publish PostgreSQL ra host port `5432` theo mặc định. Nếu port này đã bận:

```sh
POSTGRES_HOST_PORT=15432 docker compose up -d db
```

Khi chạy binary native với port khác, đồng bộ `PGPORT` trong `.env` hoặc environment.

## Samples

File mẫu nằm trong [samples](samples):

| File | Nội dung |
| --- | --- |
| `samples/arp.pcap` | Một ARP request Ethernet tối giản. |
| `samples/multi-asset.pcap` | Fixture nhiều packet gồm ARP, DHCP, UDP không phải DHCP và IPv6 bị bỏ qua; dùng cho demo parser/asset merge. |
| `samples/GenerateMultiAssetPcap.py` | Tạo lại `multi-asset.pcap`. |
| `samples/GenerateLargePcap.py` | Tạo PCAP ARP lớn để benchmark throughput. Không commit file lớn sinh ra. |

Tạo lại fixture:

```sh
python3 samples/GenerateMultiAssetPcap.py samples/multi-asset.pcap
```

Tạo PCAP nhỏ để test nhanh:

```sh
python3 samples/GenerateLargePcap.py /tmp/generated-small.pcap --count 1000
```

## Benchmark

Executable benchmark được build cùng project và yêu cầu truyền nguồn traffic rõ ràng.

Offline benchmark từ PCAP:

```sh
./build/asset-discovery-live-benchmark --pcap samples/multi-asset.pcap \
  --workers 4 \
  --batch-size 128 \
  --filter "arp or udp port 67 or udp port 68"
```

Live backend benchmark:

```sh
sudo ./build/asset-discovery-live-benchmark \
  --interface eth0 \
  --duration 30 \
  --backend auto \
  --workers 4 \
  --batch-size 128 \
  --filter "arp or udp port 67 or udp port 68"
```

Benchmark live/offline dùng chung pipeline concurrent để đo throughput, queue drop, batch drop, backend selected/fallback, copied packet count và kernel/libpcap counters khi backend hỗ trợ. Kết quả phụ thuộc OS, kernel, quyền capture, NIC, traffic mix, filter, build type và backend.

## Cấu trúc tài liệu

- [docs/design-spec/system-design.md](docs/design-spec/system-design.md): thiết kế hệ thống.
- [docs/design-spec/asset-events.md](docs/design-spec/asset-events.md): event model và event sink.
- [docs/design-spec/asset-output-contract.md](docs/design-spec/asset-output-contract.md): contract output asset.
- [docs/design-spec/docker-packaging-guide.md](docs/design-spec/docker-packaging-guide.md): hướng dẫn packaging Docker.
- [docs/build-deploy-demo-guide.md](docs/build-deploy-demo-guide.md): mục lục demo.
- [docs/demo/01-preparation.md](docs/demo/01-preparation.md) đến [docs/demo/13-cli-parameters.md](docs/demo/13-cli-parameters.md): kịch bản demo chi tiết.
- [docs/final-submission-checklist.md](docs/final-submission-checklist.md): checklist bàn giao.

Sprint notes:

- [docs/sprints/SPRINT2.md](docs/sprints/SPRINT2.md)
- [docs/sprints/SPRINT3.md](docs/sprints/SPRINT3.md)
- [docs/sprints/SPRINT4.md](docs/sprints/SPRINT4.md)

## Troubleshooting

`[CONFIG ERROR] PostgreSQL configuration is required`:

- Tạo `.env` trong thư mục chạy hoặc export `DATABASE_URL`/`PG*`.
- Nếu dùng Docker Compose mặc định, dùng `PGPASSWORD=123456`.

`[DATABASE ERROR] PostgreSQL write failed through psql`:

- Kiểm tra `psql` có trong `PATH` khi chạy native.
- Kiểm tra database đang chạy: `docker compose ps db`.
- Kiểm tra credential/port trong `.env`.

Port `5432` đã bận:

- Chạy Compose với `POSTGRES_HOST_PORT=15432`.
- Đổi `PGPORT=15432` khi binary native kết nối qua host.

Credential PostgreSQL không đúng dù đã đổi `.env`:

- Volume `postgres-data` giữ credential lần khởi tạo đầu tiên.
- Với dữ liệu demo có thể xóa volume bằng `docker compose down -v`, sau đó tạo lại.

Live capture bị permission denied:

- Chạy bằng root/sudo hoặc cấp capability capture.
- Docker live capture cần `--net=host`, `NET_ADMIN`, `NET_RAW` và interface đúng trên Linux host.

`libpcap was not found` khi configure:

- Cài `libpcap-dev`/Npcap SDK, hoặc configure với `-DASSET_DISCOVERY_REQUIRE_PCAP=OFF` nếu chỉ cần build shell.

JSON/CSV stdout không parse được:

- Event console sink cũng ghi stdout. Dùng `logs/events.ndjson` cho event machine-readable, hoặc lọc stdout tới phần summary cuối khi automation cần parse asset output.

## Giới hạn hiện tại

- Không scan chủ động và không enrichment qua network.
- Capture hiện tập trung vào Ethernet datalink.
- Hostname chủ yếu lấy từ DHCP option 12; DNS/mDNS/LLMNR/SSDP/NetBIOS hiện chủ yếu đóng góp metadata và endpoint observation.
- OUI reference metadata là danh sách nhúng tối thiểu, không phải database vendor đầy đủ.
- PostgreSQL writer phụ thuộc `psql` và database reachable tại thời điểm chạy.
- Live capture phụ thuộc quyền hệ thống, OS/backend và traffic thực tế.
