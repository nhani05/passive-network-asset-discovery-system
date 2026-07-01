# Thiết Kế Hệ Thống

Tài liệu này mô tả kiến trúc hiện tại của Passive Network Asset Discovery System theo đúng source tree C++17/CMake trong repository.

## Mục Tiêu

Hệ thống phân tích traffic thụ động từ file PCAP hoặc network interface, trích xuất observation từ ARP/DHCP/DNS bằng parser plugins, gộp observation thành asset state, rồi xuất kết quả ra terminal, file-friendly format, hoặc PostgreSQL.

## Layer Và Module Chính

| Layer | Module | Đường dẫn | Trách nhiệm |
| --- | --- | --- |
| Interface | CLI | `include/interface/cli`, `src/interface/cli` | Parse `--pcap`, `--interface`, `--duration`, `--live`, `--idle-timeout`, `--max-assets`, `--filter`, `--capture-backend`, `--output`, `--db-url`; kiểm tra quan hệ option. |
| Domain | Asset model | `include/domain`, `src/domain` | Định nghĩa `AssetObservation`, `Asset`, `AssetStore`; gộp observation theo MAC address và duy trì asset state. |
| Application | Live pipeline | `include/application/live`, `src/application/live` | Tách live capture thành producer-consumer pipeline với bounded queue, parser worker pool, aggregator single-writer, và metrics throughput/drop. |
| Application | Parser core | `include/application/parser`, `src/application/parser` | Build `PacketContext`, điều phối `ParserEngine`/`ParserRegistry`, expose parser facade `parseEthernetObservations()`. |
| Plugins | Parser plugins | `include/plugins/parser`, `src/plugins/parser` | Built-in ARP/DHCP/DNS plugins và composition root `createDefaultParserRegistry()`. |
| Infrastructure | Packet decoders | `include/infrastructure/packet`, `src/infrastructure/packet` | Decode Ethernet và ARP payload dùng chung cho context builder/plugins. |
| Infrastructure | Capture | `include/infrastructure/capture`, `src/infrastructure/capture` | Đọc PCAP offline hoặc live capture qua backend được chọn; áp dụng BPF filter; chuẩn hóa packet thành `OfflinePacket` hoặc `PacketView`. |
| Infrastructure | Output | `include/infrastructure/output`, `src/infrastructure/output` | Render asset state ở dạng table, JSON, hoặc CSV. |
| Infrastructure | Storage | `include/infrastructure/storage`, `src/infrastructure/storage` | Tạo schema tối thiểu và upsert asset vào PostgreSQL thông qua client `psql`. |
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
AssetStore::applyObservation()
        |
        v
Asset list
        |
        +--> Table/JSON/CSV renderer
        |
        +--> PostgreSQL writer nếu có --db-url, DATABASE_URL, hoặc PG*/DB* env
```

PCAP mode, live timed mode và live infinite mode dùng cùng parser, asset store, renderer, và storage writer. Điểm khác nhau nằm ở nguồn packet và stop policy. PCAP mode đọc file rồi build `AssetStore`, còn live capture đi qua concurrent pipeline để capture không bị block bởi parser hoặc aggregation.

## Capture Modes

CLI hỗ trợ ba chế độ input rõ ràng:

- PCAP offline: `--pcap <file>`, đọc hết file rồi render kết quả.
- Live timed: `--interface <name> --duration <seconds>`, capture đến khi hết thời lượng.
- Live infinite: `--interface <name> --live`, capture đến khi có stop condition.

Live capture có thể chọn backend bằng `--capture-backend auto|pcap|af-packet`. `pcap` là baseline portable qua libpcap/Npcap. `af-packet` là backend Linux dùng raw socket `AF_PACKET` với `TPACKET_V3`/`PACKET_RX_RING`, cần quyền root hoặc `CAP_NET_RAW`. `auto` thử dùng `af-packet` khi host hỗ trợ và đủ quyền, nếu không sẽ fallback về `pcap` khi libpcap khả dụng và ghi lý do fallback trong metrics.

Live infinite có thể dừng khi:

- Không có packet được chấp nhận sau BPF filter trong `--idle-timeout <seconds>`.
- Asset store đạt `--max-assets <count>`.
- Người vận hành nhấn Ctrl+C/SIGINT.

Ctrl+C trong live infinite mode chỉ đặt stop flag tối thiểu, vòng capture thoát theo hướng graceful, rồi `main` tiếp tục dùng cùng luồng render và PostgreSQL writer như PCAP/live timed. `--idle-timeout` không tính packet bị BPF filter loại ra, vì những packet này không được đưa vào pipeline phân tích.

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
│ owns AssetStore        │
└────────────────────────┘
```

Capture thread chỉ đọc packet đã qua BPF filter, chuẩn hóa thành `PacketView`, gom thành `PacketBatch`, rồi enqueue. Với backend `pcap`, bytes được copy vào owned storage trước khi đưa qua queue. Với backend `af-packet`, `PacketView` có thể trỏ trực tiếp vào mmap ring và giữ `RingBlockLease`; block chỉ được trả về kernel sau khi tất cả view trong batch xử lý xong hoặc batch bị drop. Nếu packet queue đầy, hệ thống drop batch mới, release owner/lease, và tăng counter thay vì để memory tăng vô hạn.

Parser worker pool dequeue `PacketBatch`, gọi `parseEthernetObservations()` song song, rồi enqueue `ObservationBatch`. Parser workers không cập nhật `AssetStore`.

Aggregator thread là single writer duy nhất của `AssetStore`. Sau khi capture dừng, pipeline close packet queue, drain parser workers, close observation queue, join aggregator, rồi main flow mới lấy asset list để ghi PostgreSQL hoặc render output.

Live metrics được ghi ra stderr sau capture, còn stdout vẫn chỉ chứa table/JSON/CSV asset output. Metrics gồm packet captured/enqueued/parsed, observation produced/applied, queue drop, batch drop, high watermark, elapsed time, throughput, backend requested/selected/fallback reason, copied packet count, kernel drop counters, và libpcap/AF_PACKET counters khi backend hỗ trợ.

## Xử Lý Packet

- Capture backend chỉ chấp nhận Ethernet datalink hiện tại.
- BPF filter được compile bằng libpcap trước khi đọc packet; `af-packet` attach classic BPF vào socket bằng `SO_ATTACH_FILTER` khi filter được cấu hình.
- Live capture dùng non-blocking polling với sleep ngắn khi chưa có packet để kiểm tra duration, idle timeout và stop flag ổn định.
- Parser build `PacketContext` một lần từ `ByteView` để decode Ethernet, IPv4, UDP và transport payload khi hợp lệ, tránh copy full frame trên hot path.
- Mỗi parser plugin có `match(PacketContext)` để lọc packet trước khi chạy `parse(PacketContext)`.
- Parser core chỉ biết `ParserInterface` và `ParserRegistry`; built-in plugin registration nằm trong `plugins/parser/BuiltinParserPlugins`.
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
