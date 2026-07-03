# CHƯƠNG 5. TRIỂN KHAI HỆ THỐNG

## 5.1. Giới thiệu chương

Chương này trình bày quá trình triển khai hệ thống **Passive Network Asset Discovery System** dựa trên thiết kế đã mô tả ở Chương 4. Nội dung tập trung vào công nghệ sử dụng, cấu trúc mã nguồn, cách build, cách chạy, các module đã hiện thực và cách hệ thống xử lý dữ liệu từ packet đến asset output.

Hệ thống được triển khai bằng C++17, sử dụng CMake để quản lý build, libpcap để đọc PCAP/live capture, PostgreSQL để lưu kết quả và Docker để đóng gói triển khai. Kiến trúc triển khai được chia thành nhiều thư viện nội bộ để từng phần có thể được kiểm thử riêng.

## 5.2. Công nghệ sử dụng

Các công nghệ chính được sử dụng trong hệ thống gồm:

| Công nghệ | Vai trò |
|---|---|
| C++17 | Ngôn ngữ triển khai chính |
| CMake | Quản lý cấu hình build và test |
| libpcap | Đọc file PCAP và capture packet |
| Linux AF_PACKET | Backend live capture hiệu năng cao trên Linux |
| PostgreSQL | Lưu asset snapshot và event history |
| Docker | Đóng gói ứng dụng thành container image |
| Docker Compose | Chạy demo gồm ứng dụng và database |
| CTest | Chạy bộ kiểm thử tự động |
| YAML config | Cấu hình filter, backend, output và event policy |

C++17 được lựa chọn vì phù hợp với yêu cầu xử lý hệ thống, có khả năng kiểm soát bộ nhớ, luồng và hiệu năng tốt. CMake giúp dự án dễ build trên nhiều môi trường. Docker giúp chuẩn hóa runtime và thuận tiện cho demo.

## 5.3. Cấu trúc mã nguồn

Mã nguồn được tổ chức theo các thư mục chính:

```text
.
├── CMakeLists.txt
├── Dockerfile
├── docker-compose.yml
├── configs/
├── db/
├── docs/
├── include/
├── samples/
├── scripts/
├── src/
└── tests/
```

Trong đó:

- `include/`: chứa header của các module.
- `src/`: chứa mã nguồn triển khai.
- `tests/`: chứa unit test và integration test.
- `configs/`: chứa file cấu hình YAML.
- `db/`: chứa schema PostgreSQL.
- `samples/`: chứa file PCAP mẫu.
- `docs/`: chứa tài liệu thiết kế, demo và báo cáo.

**[PLACEHOLDER HÌNH 5.1: Chèn ảnh chụp hoặc sơ đồ cây thư mục của repository, làm nổi bật các thư mục src, include, tests, configs, db, samples và docker-compose.yml]**

**Hình 5.1. Cấu trúc thư mục chính của hệ thống**

## 5.4. Cấu trúc module trong CMake

Dự án chia mã nguồn thành nhiều thư viện nội bộ. Cách chia này giúp mỗi nhóm chức năng có trách nhiệm rõ ràng và dễ kiểm thử.

Các target chính gồm:

| Target | Vai trò |
|---|---|
| `asset_discovery_domain` | Định nghĩa asset, observation và event domain |
| `asset_discovery_packet` | Xử lý Ethernet frame và ARP packet cơ bản |
| `asset_discovery_parser_core` | Parser engine, registry và packet context |
| `asset_discovery_parser_plugins` | Các plugin ARP, DHCP, DNS, NBNS, SSDP, TCP |
| `asset_discovery_parser` | Facade cho quá trình parse packet |
| `asset_discovery_monitor` | AssetMonitor điều phối asset store và event detector |
| `asset_discovery_live` | Live capture pipeline |
| `asset_discovery_output` | Renderer table, JSON, CSV và event sink |
| `asset_discovery_storage` | PostgreSQL writer |
| `asset_discovery_capture` | PCAP/libpcap và AF_PACKET capture backend |
| `asset_discovery_cli` | Phân tích tham số dòng lệnh |
| `asset_discovery_config` | Load và merge cấu hình |
| `asset-discovery` | Executable chính |
| `asset-discovery-live-benchmark` | Executable benchmark live pipeline |

Lệnh build cơ bản:

```bash
cmake -S . -B build
cmake --build build
```

Khi cần yêu cầu bắt buộc có libpcap:

```bash
cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=ON
cmake --build build
```

## 5.5. Triển khai CLI

CLI là điểm vào chính của người dùng. Chương trình hỗ trợ hai chế độ chạy:

```bash
./build/asset-discovery --pcap samples/arp.pcap
sudo ./build/asset-discovery --interface eth0
```

Các tham số thông dụng:

```bash
--pcap <file>
--interface <name>
--config <file>
--profile <name>
--filter <bpf>
--capture-backend <auto|pcap|af-packet>
--output <table|json|csv>
--help
--version
```

CLI kiểm tra tính hợp lệ của tham số trước khi hệ thống bắt đầu capture. Ví dụ, người dùng không được truyền đồng thời `--pcap` và `--interface`. Nếu thiếu input, chương trình báo lỗi và dừng.

**[PLACEHOLDER HÌNH 5.2: Chèn ảnh chụp màn hình kết quả chạy lệnh ./build/asset-discovery --help]**

**Hình 5.2. Giao diện trợ giúp dòng lệnh của chương trình**

## 5.6. Triển khai cấu hình

Hệ thống hỗ trợ file cấu hình YAML trong thư mục `configs`. Ví dụ `configs/default.yaml`:

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

Người dùng có thể chạy với profile:

```bash
./build/asset-discovery --profile pcap --pcap samples/arp.pcap
sudo ./build/asset-discovery --profile live --interface eth0
```

Thiết kế cấu hình tách biệt rõ giữa policy/runtime và nguồn dữ liệu. File YAML không chứa `pcap path`, `interface` hoặc password database. Các thông tin nhạy cảm được đặt trong `.env` hoặc biến môi trường.

## 5.7. Triển khai capture layer

Capture layer nằm trong module `asset_discovery_capture`. Module này hỗ trợ đọc packet từ file PCAP và live interface.

Ở chế độ PCAP offline, hệ thống đọc tuần tự các packet trong file, giữ timestamp packet và bytes gốc để đưa vào parser. Chế độ này phù hợp cho kiểm thử tự động vì dữ liệu đầu vào ổn định và không phụ thuộc traffic thật.

Ở chế độ live capture, hệ thống mở network interface và đọc packet trực tiếp. Backend có thể là:

- `pcap`: dùng libpcap.
- `af-packet`: dùng Linux AF_PACKET.
- `auto`: tự chọn backend phù hợp, có fallback khi backend ưu tiên không khả dụng.

Live capture cần quyền hệ thống. Khi chạy native có thể cần `sudo`. Khi chạy trong container cần `--net=host` và capability `NET_ADMIN`, `NET_RAW`.

## 5.8. Triển khai packet context

Trước khi chạy plugin, hệ thống xây dựng `PacketContext` từ bytes của Ethernet frame. `PacketContext` giúp parser truy cập các thông tin đã decode theo cách thống nhất:

- Ethernet header.
- IPv4 header nếu packet là IPv4.
- UDP header nếu packet dùng UDP.
- Payload tầng transport.
- Timestamp packet.

Parser luôn kiểm tra độ dài buffer trước khi đọc dữ liệu. Đây là điểm quan trọng vì packet trong PCAP hoặc live traffic có thể bị cắt ngắn, sai định dạng hoặc không thuộc giao thức được hỗ trợ.

## 5.9. Triển khai parser plugin

Parser được triển khai theo mô hình plugin. Mỗi plugin kiểm tra packet có phù hợp hay không, sau đó trích xuất observation.

### 5.9.1. ARP plugin

ARP plugin phân tích các gói ARP để lấy thông tin IP và MAC. Từ ARP Request hoặc ARP Reply, hệ thống có thể tạo observation chứa:

- MAC nguồn.
- IP nguồn.
- MAC đích nếu có.
- IP đích.
- Nguồn phát hiện `arp`.

Ví dụ, khi quan sát packet từ MAC `02:42:ac:11:00:02` và IP `192.168.1.10`, hệ thống tạo observation tương ứng và chuyển đến `AssetMonitor`.

### 5.9.2. DHCP plugin

DHCP plugin phân tích lưu lượng UDP port 67/68. Plugin này có thể trích xuất:

- MAC client.
- IP được cấp hoặc IP liên quan.
- Hostname từ DHCP option 12 nếu có.
- Nguồn phát hiện `dhcp`.

DHCP là nguồn dữ liệu quan trọng vì có thể cung cấp hostname, điều mà ARP không có.

### 5.9.3. DNS plugin

DNS plugin ghi nhận endpoint quan sát được từ truy vấn DNS. DNS không luôn cung cấp MAC trực tiếp ở tầng ứng dụng, nhưng khi kết hợp với Ethernet/IP context, hệ thống có thể bổ sung nguồn phát hiện và metadata liên quan đến hoạt động mạng của thiết bị.

### 5.9.4. NBNS, SSDP và TCP plugin

Các plugin NBNS, SSDP và TCP giúp mở rộng khả năng quan sát. NBNS có thể liên quan đến tên máy trong môi trường Windows. SSDP thường xuất hiện trong các thiết bị UPnP/IoT. TCP plugin hỗ trợ ghi nhận endpoint theo lưu lượng TCP khi phù hợp với mục tiêu quan sát.

**[PLACEHOLDER HÌNH 5.3: Chèn sơ đồ minh họa các parser plugin nhận PacketContext và sinh AssetObservation]**

**Hình 5.3. Cơ chế parser plugin sinh observation**

## 5.10. Triển khai AssetStore

`AssetStore` lưu danh sách asset đã phát hiện. Khi nhận observation, store cập nhật asset theo MAC address.

Ví dụ một asset sau khi tổng hợp có dạng:

```json
{
  "mac_address": "02:42:ac:11:00:03",
  "ip_addresses": ["192.168.1.20"],
  "hostname": "laptop-user",
  "first_seen": "1699606802.2000",
  "last_seen": "1699606803.3000",
  "discovery_sources": ["arp", "dhcp"]
}
```

Trường `ip_addresses` là danh sách vì một thiết bị có thể thay đổi IP. Trường `discovery_sources` cũng là danh sách vì cùng một thiết bị có thể được phát hiện qua nhiều giao thức khác nhau.

## 5.11. Triển khai AssetMonitor và event detection

`AssetMonitor` nhận observation từ parser và thực hiện hai nhiệm vụ:

- Phát hiện sự kiện thông qua `AssetEventDetector`.
- Cập nhật trạng thái asset trong `AssetStore`.

Các event được hỗ trợ gồm:

| Event | Ý nghĩa |
|---|---|
| `new_asset` | MAC mới xuất hiện |
| `mac_changed_for_ip` | IP được gắn với MAC khác |
| `ip_changed_for_mac` | MAC xuất hiện với IP mới |
| `ip_mac_flip_flop` | IP/MAC đổi qua lại trong thời gian ngắn |
| `asset_reappeared` | Thiết bị xuất hiện lại sau thời gian dài |
| `hostname_learned` | Học được hostname mới |
| `hostname_changed` | Hostname thay đổi |
| `ethernet_arp_mac_mismatch` | MAC Ethernet khác MAC trong ARP |
| `non_local_source_ip` | IP nguồn không thuộc dải local |

Event có thể được in ra stdout, ghi vào NDJSON, syslog hoặc PostgreSQL.

## 5.12. Triển khai output renderer

Hệ thống có ba định dạng output summary.

### 5.12.1. Table output

Table output phù hợp để quan sát trực tiếp:

```bash
./build/asset-discovery --pcap samples/arp.pcap --output table
```

Các cột chính gồm MAC, IPs, Hostname, First Seen, Last Seen và Sources.

**[PLACEHOLDER HÌNH 5.4: Chèn ảnh chụp màn hình kết quả chạy table output với samples/arp.pcap hoặc samples/multi-asset.pcap]**

**Hình 5.4. Kết quả phát hiện tài sản ở định dạng bảng**

### 5.12.2. JSON output

JSON output phù hợp cho tích hợp:

```bash
./build/asset-discovery --pcap samples/arp.pcap --output json
```

Kết quả là một mảng asset object. Field `hostname` có thể bị bỏ qua nếu chưa biết.

### 5.12.3. CSV output

CSV output phù hợp cho lưu trữ hoặc nhập vào bảng tính:

```bash
./build/asset-discovery --pcap samples/arp.pcap --output csv
```

CSV luôn có header:

```csv
mac_address,ip_addresses,hostname,first_seen,last_seen,discovery_sources
```

## 5.13. Triển khai event log NDJSON

Event log được ghi mặc định vào:

```text
logs/events.ndjson
```

Mỗi dòng là một JSON object độc lập. Ví dụ:

```json
{"ts":"1.0","event_type":"new_asset","severity":"info","ip":"192.168.1.12","mac":"aa:bb:cc:dd:ee:ff","protocol":"arp","message":"New asset discovered","metadata":{}}
```

Người dùng có thể đổi đường dẫn bằng biến môi trường:

```bash
export ASSET_DISCOVERY_EVENTS_JSON=custom_logs/events.ndjson
```

Định dạng NDJSON giúp log dễ append và dễ phân tích bằng công cụ dòng lệnh hoặc hệ thống log.

## 5.14. Triển khai PostgreSQL writer

Schema PostgreSQL nằm trong `db/schema.sql`. Hệ thống sử dụng hai bảng chính:

- `assets`: lưu trạng thái cuối của tài sản.
- `asset_events`: lưu lịch sử sự kiện.

Cấu hình kết nối database có thể đặt trong `.env`:

```env
PGHOST=localhost
PGPORT=5432
PGDATABASE=asset_discovery
PGUSER=postgres
PGPASSWORD=123456
```

Khi chạy, hệ thống ghi dữ liệu bằng PostgreSQL client `psql`. Với bảng `assets`, hệ thống sử dụng upsert để cập nhật bản ghi theo `mac_address` nếu asset đã tồn tại.

Ví dụ truy vấn kiểm tra:

```bash
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets;"
```

**[PLACEHOLDER HÌNH 5.5: Chèn ảnh chụp kết quả truy vấn bảng assets trong PostgreSQL sau khi chạy demo PCAP]**

**Hình 5.5. Dữ liệu tài sản được lưu trong PostgreSQL**

## 5.15. Triển khai live capture pipeline

Live capture pipeline được triển khai trong module `asset_discovery_live`. Pipeline tách các giai đoạn xử lý thành nhiều thread:

- Capture thread đọc packet.
- Parser worker pool phân tích packet song song.
- Aggregator thread cập nhật asset store.
- Event writer thread ghi event.

Các queue trong pipeline có giới hạn dung lượng. Nếu queue đầy, hệ thống ghi nhận số batch bị drop thay vì để bộ nhớ tăng không kiểm soát. Khi dừng live capture, pipeline đóng queue, drain dữ liệu còn lại, join thread và flush sink.

Lệnh chạy live:

```bash
sudo ./build/asset-discovery --profile live --interface eth0
```

Nếu muốn chọn backend:

```bash
sudo ./build/asset-discovery --interface eth0 --capture-backend af-packet
```

## 5.16. Triển khai Docker

Dockerfile sử dụng multi-stage build. Stage build cài công cụ biên dịch và libpcap development package. Stage runtime chỉ chứa binary, libpcap runtime và PostgreSQL client cần thiết.

Build image:

```bash
docker build -t passive-asset-discovery:latest .
```

Chạy PCAP trong container:

```bash
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  passive-asset-discovery:latest \
  --pcap /samples/arp.pcap \
  --output table
```

Chạy với Docker Compose:

```bash
docker compose up --build pcap-demo
```

Live capture trong Docker:

```bash
CAPTURE_INTERFACE=eth0 docker compose --profile live run --rm live-capture
```

**[PLACEHOLDER HÌNH 5.6: Chèn ảnh chụp quá trình chạy docker compose up --build pcap-demo và container kết nối PostgreSQL thành công]**

**Hình 5.6. Demo hệ thống bằng Docker Compose**

## 5.17. Triển khai sample PCAP

Thư mục `samples/` chứa các file PCAP mẫu phục vụ demo:

- `arp.pcap`: PCAP đơn giản chứa gói ARP.
- `multi-asset.pcap`: PCAP gồm nhiều asset, có ARP và DHCP.
- `generated-5m-arp.pcap`: dữ liệu sinh ra để kiểm tra tải lớn hơn.

Ngoài ra, thư mục này có script Python để sinh PCAP phục vụ benchmark hoặc kiểm thử.

## 5.18. Xử lý lỗi trong triển khai

Hệ thống xử lý các lỗi thường gặp như:

- Không truyền nguồn input.
- File PCAP không tồn tại.
- Filter BPF sai.
- Không có quyền mở interface.
- Backend capture không khả dụng.
- Thiếu cấu hình database.
- PostgreSQL không kết nối được.
- Config YAML có key không hợp lệ.

Các lỗi runtime được in ra stderr, còn stdout được giữ cho output chính để tránh làm hỏng JSON/CSV.

## 5.19. Tổng kết chương

Chương này đã trình bày quá trình triển khai hệ thống Passive Network Asset Discovery System. Hệ thống được hiện thực bằng C++17/CMake, chia thành nhiều module như CLI, config, capture, parser, discovery, event, output, storage và live pipeline. Hệ thống có thể chạy với PCAP offline, live capture, xuất nhiều định dạng kết quả, ghi PostgreSQL và triển khai bằng Docker.

Các nội dung triển khai trong chương này là cơ sở để tiến hành kiểm thử và đánh giá hệ thống ở Chương 6.
