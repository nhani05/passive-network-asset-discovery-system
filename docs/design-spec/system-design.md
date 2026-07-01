# Thiết Kế Hệ Thống

Tài liệu này mô tả kiến trúc hiện tại của Passive Network Asset Discovery System theo đúng source tree C++17/CMake trong repository.

## Mục Tiêu

Hệ thống phân tích traffic thụ động từ file PCAP hoặc network interface, trích xuất observation từ ARP/DHCP/DNS bằng parser plugins, gộp observation thành asset state, rồi xuất kết quả ra terminal, file-friendly format, hoặc PostgreSQL.

## Layer Và Module Chính

| Layer | Module | Đường dẫn | Trách nhiệm |
| --- | --- | --- |
| Interface | CLI | `include/pnad/cli`, `src/cli` | Parse `--pcap`, `--interface`, `--filter`, `--capture-backend`, `--output`, và các option điều chỉnh; kiểm tra quan hệ option và từ chối các cờ cũ. |
| Domain / Discovery | Asset / Discovery | `include/pnad/discovery`, `src/discovery` | Định nghĩa `AssetObservation`, `Asset`, `AssetStore`, và `AssetMonitor`; quản lý danh sách asset và gộp observation theo MAC. |
| Application | Live pipeline | `include/pnad/app`, `src/app` | Tách live capture thành producer-consumer pipeline với bounded queue, parser worker pool, aggregator single-writer, event writer, và metrics throughput/drop. |
| Domain / Event | Event detection | `include/pnad/event`, `src/event` | Định nghĩa `AssetEvent`, `AssetEventDetector`, `EventRateLimiter` và các event sinks để phát hiện và ghi nhận sự kiện thay đổi IP/MAC hoặc các bất thường. |
| Packet / Parser | Packet Processing | `include/pnad/packet`, `src/packet` | Decode Ethernet/ARP frame; điều phối `ParserEngine`/`ParserRegistry` và chứa các built-in parser plugins (ARP, DHCP, DNS). |
| Infrastructure | Capture | `include/pnad/capture`, `src/capture` | Đọc PCAP offline hoặc live capture qua backend được chọn (PCAP / AF_PACKET); áp dụng BPF filter. |
| Infrastructure | Storage | `include/pnad/storage`, `src/storage` | Lưu trữ asset và asset_events vào PostgreSQL thông qua client `psql`. |
| System | Concurrency & Utils | `include/pnad/system` | Chứa các lớp hỗ trợ concurrency như `BoundedQueue` và kiểu span byte `ByteView`. |
| Composition | Main | `src/main.cpp` | Nối CLI, capture, parser facade, asset store, output renderer, và storage writer thành một luồng chạy. |

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
PacketContext { raw bytes, timestamp, Ethernet, IPv4?, UDP?, payload }
        |
        v
ParserEngine + ParserRegistry
        |
        +--> ARPPlugin  match() -> parse()
        +--> DHCPPlugin match() -> parse()
        +--> DNSPlugin  match() -> parse()
        |
        v
AssetObservation { macAddress, ipAddress, hostname, sourceId, eventType, confidence, metadata, timestamp }
        |
        v
AssetMonitor
        |
        +--> AssetEventDetector -> EventDispatcher (mặc định gửi tới các sink như stdout, NDJSON, syslog, và database)
        |
        v
AssetStore::applyObservation()
        |
        v
Asset list
        |
        +--> Table/JSON/CSV renderer (kết quả summary)
        |
        +--> PostgreSQL writer (bắt buộc, tự động lưu thông qua cấu hình môi trường DATABASE_URL hoặc PG*/DB*)
```

PCAP mode và live mode dùng cùng parser, asset monitor, renderer, và storage writer. Điểm khác nhau nằm ở nguồn packet và stop policy. PCAP mode đọc file rồi replay observation qua `AssetMonitor`, còn live capture chạy live vô hạn thông qua concurrent pipeline để capture không bị block bởi parser, aggregation hoặc event output.

## Capture Modes

CLI hỗ trợ hai chế độ input:

- PCAP offline: `--pcap <file>`, đọc hết file, ghi nhận event mặc định, lưu database, và render kết quả summary cuối cùng.
- Live capture: `--interface <name>`, chạy live vô hạn cho tới khi có tín hiệu dừng SIGINT/SIGTERM hoặc lỗi runtime nghiêm trọng.

Live capture có thể chọn backend bằng `--capture-backend auto|pcap|af-packet`. `pcap` là portable qua libpcap. `af-packet` là backend Linux dùng raw socket `AF_PACKET` với `TPACKET_V3`/`PACKET_RX_RING`, cần quyền root hoặc `CAP_NET_RAW`. `auto` thử dùng `af-packet` khi host hỗ trợ và đủ quyền, nếu không sẽ fallback về `pcap` khi libpcap khả dụng và ghi lý do fallback trong metrics.

Live capture dừng khi nhận tín hiệu SIGINT hoặc SIGTERM (ví dụ: Ctrl+C hoặc lệnh kill). Vòng capture thoát theo hướng graceful, các queue và sink được flush và drain sạch sẽ, sau đó `main` thực hiện ghi asset snapshot cuối cùng vào PostgreSQL và render kết quả output summary.

## Live Capture Concurrency

Live capture dùng mô hình producer-consumer để tách stage đọc packet khỏi stage xử lý:

```text
┌────────────────┐
│ Capture Thread │
│ pcap/af-packet │
└───────┬────────┘
        │ PacketBatch
        v
┌────────────────────────┐
│ Bounded Packet Queue   │
│ mutex + condition var  │
└───────┬────────────────┘
        │
        v
┌────────────────────────┐
│ Parser Worker Pool     │
│ N std::thread workers  │
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
└───────┬────────────────┘
        │ AssetEvent
        v
┌────────────────────────┐
│ Bounded Event Queue    │
└───────┬────────────────┘
        v
┌────────────────────────┐
│ Event Writer Thread    │
└────────────────────────┘
```

Capture thread chỉ đọc packet đã qua BPF filter, chuẩn hóa thành `PacketView`, gom thành `PacketBatch`, rồi enqueue. Với backend `pcap`, bytes được copy vào owned storage trước khi đưa qua queue. Với backend `af-packet`, `PacketView` có thể trỏ trực tiếp vào mmap ring và giữ `RingBlockLease`; block chỉ được trả về kernel sau khi tất cả view trong batch xử lý xong hoặc batch bị drop. Nếu packet queue đầy, hệ thống drop batch mới, release owner/lease, và tăng counter thay vì để memory tăng vô hạn.

Parser worker pool dequeue `PacketBatch`, gọi `parseEthernetObservations()` song song, rồi enqueue `ObservationBatch`. Parser workers không cập nhật `AssetStore`.

Aggregator thread là single writer duy nhất của `AssetMonitor` và `AssetStore`. Aggregator detect event trước khi apply observation, đẩy event vào bounded event queue bằng non-blocking push, rồi event writer thread ghi ra các default event sinks (stdout, NDJSON, syslog, database). Sau khi capture dừng, pipeline close packet queue, drain parser workers, close observation queue, join aggregator, close/drain event queue, flush sink, rồi main flow mới lấy asset list để ghi PostgreSQL và render output summary.

Live metrics được ghi ra stderr sau capture, còn stdout mặc định chứa realtime event lines trong lúc chạy, và in summary table/JSON/CSV sau khi dừng. Metrics gồm packet captured/enqueued/parsed, observation produced/applied, event produced/enqueued/drop counters, queue drop, batch drop, high watermark, elapsed time, throughput, backend requested/selected/fallback reason, copied packet count, kernel drop counters, và libpcap/AF_PACKET counters khi backend hỗ trợ.

## Xử Lý Packet

- Capture backend chỉ chấp nhận Ethernet datalink hiện tại.
- BPF filter được compile bằng libpcap trước khi đọc packet; `af-packet` attach classic BPF vào socket bằng `SO_ATTACH_FILTER` khi filter được cấu hình.
- Live capture dùng non-blocking polling với sleep ngắn khi chưa có packet để kiểm tra stop flag từ tín hiệu SIGINT/SIGTERM ổn định.
- Parser build `PacketContext` một lần từ `ByteView` để decode Ethernet, IPv4, UDP và transport payload khi hợp lệ, tránh copy full frame trên hot path.
- Mỗi parser plugin có `match(PacketContext)` để lọc packet trước khi chạy `parse(PacketContext)`.
- Parser core chỉ biết `ParserInterface` và `ParserRegistry`; built-in plugin registration nằm trong `pnad/packet/BuiltinParserPlugins`.
- Registry hiện là static built-in registry, không dùng dynamic `.so`/`.dll`, để tránh ABI/runtime complexity trong scope C++17 hiện tại.
- Built-in plugin hiện có: ARP, DHCP, DNS endpoint observation.
- Parser plugins chạy trong worker pool của live capture, nên `match(PacketContext)` và `parse(PacketContext)` phải stateless hoặc tự thread-safe; plugin không được mutate shared state nếu không tự đồng bộ. Observation tạo ra phải copy dữ liệu cần giữ lâu, không được giữ pointer vào `PacketView` hoặc AF_PACKET ring.
- Parser kiểm tra độ dài buffer trước khi đọc field; packet chưa hỗ trợ hoặc bị cắt ngắn được bỏ qua an toàn.
- Parser không ghi warning vào stdout JSON; lỗi capture được trả về cho `main` để in trên stderr.

## Asset State

Asset được định danh bằng MAC address đã chuẩn hóa chữ thường. Một asset có thể có nhiều IP vì DHCP hoặc traffic quan sát được theo thời gian có thể thay đổi.

Quy tắc merge:

- MAC mới tạo asset mới.
- Observation cùng MAC cập nhật `last_seen`.
- Timestamp sớm hơn cập nhật `first_seen`.
- IP không rỗng được thêm vào `ip_addresses`.
- Hostname DHCP không rỗng cập nhật `hostname`.
- Source protocol dạng lowercase text id được thêm vào `discovery_sources`, ví dụ `arp`, `dhcp`, `dns`.
- Observation có `eventType`, `confidence`, và metadata key/value để hỗ trợ giải thích nguồn dữ liệu và mở rộng protocol mà không thêm field riêng cho từng protocol.

## PostgreSQL

Schema nằm ở `db/schema.sql`. Bảng `assets` dùng `mac_address` làm khóa chính và lưu:

- `ip_addresses` dạng `text[]`.
- `hostname` có thể null.
- `first_seen`, `last_seen` dạng text cùng format với output.
- `discovery_sources` dạng `text[]`.
- `updated_at` theo thời điểm ghi database.

Writer dùng câu lệnh `INSERT ... ON CONFLICT (mac_address) DO UPDATE` để chạy lại cùng PCAP mà không tạo duplicate MAC.

Bảng `asset_events` tự động lưu lịch sử event khi cấu hình database hợp lệ. Bảng này tách khỏi `assets`: `assets` là trạng thái cuối, còn `asset_events` là timeline những thay đổi như `new_asset`, `mac_changed_for_ip`, `ip_mac_flip_flop`, mismatch ARP/Ethernet hoặc non-local source IP.

## Docker Và Runtime

`Dockerfile` dùng multi-stage build:

- Stage build cài CMake, compiler, `pkg-config`, và `libpcap-dev`.
- Stage runtime cài `libpcap0.8`, `postgresql-client`, `iputils-ping`, copy binary, và chạy mặc định bằng user `asset`.

PCAP mode trong container không cần quyền đặc biệt. Live capture trong container cần Linux host, `--net=host`, `NET_ADMIN`, `NET_RAW`, và interface thật trên host. Backend `af-packet` phụ thuộc Linux kernel packet socket, nên Docker Desktop trên macOS/Windows không phải môi trường benchmark phù hợp.

## Vì Sao PCAP Là Baseline Test

PCAP fixture ổn định, không cần quyền capture, không phụ thuộc traffic thật, và cho phép kiểm tra deterministic output trong CTest. Live capture thật vẫn là smoke test thủ công vì phụ thuộc hệ điều hành, quyền, interface, và traffic tại thời điểm demo.

Benchmark throughput dùng executable `asset-discovery-live-benchmark`. Chế độ offline đọc PCAP do người dùng cung cấp rồi chạy packet qua cùng concurrent parser/aggregation pipeline mà không cần quyền live capture. Chế độ live backend comparison chạy cùng interface, duration, filter, worker count, và batch size cho `pcap` hoặc `af-packet`, rồi báo packet/s, copied packet count, kernel drops, queue drops, batch drops, và drop rate. Mục tiêu 1M packet/s là mục tiêu benchmark cho Release build trong điều kiện đã ghi rõ OS, kernel, build type, traffic source, packet mix, filter, backend, runtime environment, và quyền capture; không phải bảo đảm cho mọi NIC, OS, hoặc mọi loại traffic.

## Giới Hạn Hiện Tại

- Hệ thống chỉ passive monitoring, không scan chủ động.
- Chỉ decode Ethernet với ARP, DHCP metadata cơ bản, và DNS endpoint observation thụ động.
- DHCP hostname chỉ xuất hiện khi traffic có option tương ứng.
- DNS plugin không gán hostname từ query name và không thực hiện resolver/enrichment chủ động.
- Live capture cần quyền hệ thống và có thể khác nhau giữa Linux, macOS, Windows/Npcap, và Docker Desktop.
- PostgreSQL writer phụ thuộc client `psql` trong `PATH` và database đang reachable.
