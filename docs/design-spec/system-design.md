# Thiết Kế Hệ Thống

Tài liệu này mô tả kiến trúc hiện tại của Passive Network Asset Discovery System theo source tree C++17/CMake trong repository.

## Mục Tiêu

Hệ thống phân tích traffic thụ động từ file PCAP hoặc network interface, trích xuất observation từ ARP/DHCP/DNS, gộp observation thành asset state, in realtime event khi phát hiện asset mới, render summary ra terminal, và lưu asset inventory vào SQLite.

Database chỉ lưu asset. Event/log không được ghi ra `events.json`, NDJSON, syslog, PostgreSQL, hoặc bảng event log.

## Layer Và Module Chính

| Layer | Module | Đường dẫn | Trách nhiệm |
| --- | --- | --- | --- |
| Interface | CLI | `include/pnad/cli`, `src/cli` | Parse `--pcap`, SQLite path, output format, và từ chối các cờ cũ. |
| Application | Config | `include/pnad/config`, `src/config` | Load defaults, `configs/default.yaml`, `.env`/environment, và CLI overrides thành `AppConfig` đã validate. |
| Domain / Discovery | Asset / Discovery | `include/pnad/discovery`, `src/discovery` | Định nghĩa `AssetObservation`, `Asset`, `AssetStore`, `AssetMonitor`; gộp observation theo MAC và phát event `new_asset`. |
| Domain / Event | New asset event | `include/pnad/event`, `src/event` | Định nghĩa `AssetEvent`, `AssetEventDetector`, `EventDispatcher`, và console sink cho event asset mới. |
| Packet / Parser | Packet Processing | `include/pnad/packet`, `src/packet` | Decode Ethernet/ARP frame; điều phối parser engine và các built-in parser plugins ARP, DHCP, DNS. |
| Application | Live pipeline | `include/pnad/app`, `src/app` | Tách live capture thành capture thread, packet queue, parser worker pool, observation queue, và aggregator single-writer. |
| Infrastructure | Capture | `include/pnad/capture`, `src/capture` | Đọc PCAP offline hoặc live capture qua backend được chọn; áp dụng BPF filter. |
| Infrastructure | Storage | `include/pnad/storage`, `src/storage` | Khởi tạo/migrate SQLite và upsert asset inventory vào bảng `assets`. |
| Composition | Main | `src/main.cpp` | Nối CLI, config, capture, parser facade, asset monitor, renderer, và SQLite writer thành một luồng chạy. |

## Luồng Dữ Liệu

```text
PCAP file hoặc network interface
        |
        v
PacketCaptureBackend
        |
        v
OfflinePacket { timestamp, linkType, bytes }
        |
        v
parseEthernetObservations()
        |
        v
PacketContextBuilder
        |
        v
ParserEngine + ParserRegistry
        |
        +--> ARPPlugin
        +--> DHCPPlugin
        +--> DNSPlugin
        |
        v
AssetObservation
        |
        v
AssetMonitor
        |
        +--> nếu MAC mới: EventDispatcher -> stdout / GUI log
        |
        v
AssetStore::applyObservation()
        |
        v
Asset list
        |
        +--> Table/JSON/CSV renderer
        |
        +--> SQLiteWriter -> assets
```

PCAP mode và live mode dùng cùng parser, asset monitor, renderer, và SQLite writer. PCAP mode đọc hết file rồi render summary. Live capture chạy tới khi nhận `SIGINT`/`SIGTERM` hoặc lỗi runtime nghiêm trọng.

## Capture Modes

CLI hỗ trợ một chế độ input:

- PCAP offline: `--pcap <file>`, đọc hết file, in event asset mới trong lúc phân tích, lưu asset vào SQLite, rồi render summary cuối.

## Application Configuration

Thứ tự merge cấu hình:

```text
built-in defaults
  -> configs/default.yaml nếu tồn tại
  -> .env/process environment
  -> CLI overrides
```

Schema YAML hiện chỉ hỗ trợ:

- `output.format`

Capture source, capture backend, và BPF filter không nằm trong YAML. Capture cố định ở PCAP mode; file đầu vào đi qua `--pcap`. SQLite path đi qua `--sqlite`, `SQLITE_DATABASE_PATH`, hoặc `.env`. Các section `capture`, `database`, `events`, và `network` không còn được hỗ trợ.

## Live Capture Concurrency

Live capture dùng producer-consumer để tách đọc packet khỏi xử lý:

```text
┌────────────────┐
│ Capture Thread │
└───────┬────────┘
        │ PacketBatch
        v
┌────────────────────────┐
│ Bounded Packet Queue   │
└───────┬────────────────┘
        v
┌────────────────────────┐
│ Parser Worker Pool     │
└───────┬────────────────┘
        │ ObservationBatch
        v
┌────────────────────────┐
│ Bounded Observation    │
│ Queue                  │
└───────┬────────────────┘
        v
┌────────────────────────┐
│ Asset Aggregator       │
│ owns AssetMonitor      │
└────────────────────────┘
```

Aggregator là single writer duy nhất của `AssetMonitor` và `AssetStore`. Khi observation tạo MAC mới, monitor phát event `new_asset` ra console/UI. Khi observation thuộc asset đã có, store cập nhật `last_seen` và các field asset liên quan mà không phát event log riêng.

Live metrics được ghi ra stderr sau capture. Stdout dành cho realtime event lines và summary table/JSON/CSV.

## Asset State

Asset được định danh bằng MAC address chuẩn hóa chữ thường.

Quy tắc merge:

- MAC mới tạo asset mới và phát event `new_asset`.
- Observation cùng MAC cập nhật `last_seen`.
- Timestamp sớm hơn cập nhật `first_seen`.
- IP không rỗng được thêm vào `ip_addresses`.
- Hostname DHCP không rỗng cập nhật `hostname`.
- Source protocol dạng lowercase text id được thêm vào `discovery_sources`.
- Metadata observation được merge vào metadata asset.

## SQLite

SQLite schema được migrate trong `src/storage/SQLiteWriter.cpp`. Bảng chính:

- `assets`: lưu asset inventory theo `mac_address`.
- `app_settings`: lưu cấu hình GUI.
- `analysis_sessions`: lưu metadata phiên phân tích cho GUI/service.

Migration hiện tại drop bảng `asset_events` nếu database cũ còn tồn tại. Điều này giữ đúng contract: database không lưu event log.

## Docker Và Runtime

`Dockerfile` dùng multi-stage build:

- Stage build cài CMake, compiler, `pkg-config`, `libpcap-dev`, `libsqlite3-dev`, và Qt dependency.
- Stage runtime cài `libpcap0.8`, `libsqlite3-0`, Qt runtime dependency, copy binary, và chạy bằng user `asset`.

Docker Compose hiện có:

- `pcap-demo`: chạy sample PCAP và ghi SQLite vào volume `/work/data/pnad.db`.
- `assetd`: chạy backend service với SQLite path `/data/pnad.db`.

Không còn service PostgreSQL trong Compose.

## Giới Hạn Hiện Tại

- Hệ thống chỉ passive monitoring, không scan chủ động.
- Chỉ decode Ethernet với ARP, DHCP metadata cơ bản, và DNS endpoint observation thụ động.
- DHCP hostname chỉ xuất hiện khi traffic có option tương ứng.
- DNS plugin không gán hostname từ query name và không thực hiện resolver/enrichment chủ động.
- Live capture cần quyền hệ thống và có thể khác nhau giữa Linux, macOS, Windows/Npcap, và Docker Desktop.
