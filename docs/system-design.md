# Thiết Kế Hệ Thống

Tài liệu này mô tả kiến trúc hiện tại của Passive Network Asset Discovery System theo đúng source tree C++17/CMake trong repository.

## Mục Tiêu

Hệ thống phân tích traffic thụ động từ file PCAP hoặc network interface, trích xuất observation từ ARP/DHCP, gộp observation thành asset state, rồi xuất kết quả ra terminal, file-friendly format, hoặc PostgreSQL.

## Module Chính

| Module | Đường dẫn | Trách nhiệm |
| --- | --- | --- |
| CLI | `include/cli`, `src/cli` | Parse `--pcap`, `--interface`, `--duration`, `--filter`, `--output`, `--db-url`; kiểm tra quan hệ option. |
| Capture | `include/capture`, `src/capture` | Đọc PCAP offline hoặc live capture qua libpcap/Npcap; áp dụng BPF filter; chuẩn hóa packet thành `OfflinePacket`. |
| Parser | `include/parser`, `src/parser` | Decode Ethernet, ARP, IPv4/UDP/DHCP; trả về `AssetObservation`; bỏ qua packet chưa hỗ trợ hoặc bị cắt ngắn. |
| Asset | `include/asset`, `src/asset` | Gộp observation theo MAC address; duy trì IP set, hostname, first_seen, last_seen, và discovery_sources. |
| Output | `include/output`, `src/output` | Render asset state ở dạng table, JSON, hoặc CSV. |
| Storage | `include/storage`, `src/storage` | Tạo schema tối thiểu và upsert asset vào PostgreSQL thông qua client `psql`. |
| Orchestration | `src/main.cpp` | Nối CLI, capture, parser, asset store, output renderer, và storage writer thành một luồng chạy. |

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
AssetObservation { macAddress, ipAddress, hostname, source, timestamp }
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

PCAP mode và live capture dùng cùng parser, asset store, renderer, và storage writer. Điểm khác nhau duy nhất là nguồn packet trong `PacketCaptureBackend`.

## Xử Lý Packet

- Capture backend chỉ chấp nhận Ethernet datalink hiện tại.
- BPF filter được compile bằng libpcap trước khi đọc packet.
- Parser kiểm tra độ dài buffer trước khi đọc field.
- Packet không phải ARP/DHCP hoặc packet chưa hỗ trợ được bỏ qua an toàn.
- Parser không ghi warning vào stdout JSON; lỗi capture được trả về cho `main` để in trên stderr.

## Asset State

Asset được định danh bằng MAC address đã chuẩn hóa chữ thường. Một asset có thể có nhiều IP vì DHCP hoặc traffic quan sát được theo thời gian có thể thay đổi.

Quy tắc merge:

- MAC mới tạo asset mới.
- Observation cùng MAC cập nhật `last_seen`.
- Timestamp sớm hơn cập nhật `first_seen`.
- IP không rỗng được thêm vào `ip_addresses`.
- Hostname DHCP không rỗng cập nhật `hostname`.
- Source protocol được thêm vào `discovery_sources`.

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

PCAP mode trong container không cần quyền đặc biệt. Live capture trong container cần Linux host, `--net=host`, `NET_ADMIN`, `NET_RAW`, và interface thật trên host.

## Vì Sao PCAP Là Baseline Test

PCAP fixture ổn định, không cần quyền capture, không phụ thuộc traffic thật, và cho phép kiểm tra deterministic output trong CTest. Live capture vẫn dùng cùng pipeline nhưng được xem là smoke test thủ công vì phụ thuộc hệ điều hành, quyền, interface, và traffic tại thời điểm demo.

## Giới Hạn Hiện Tại

- Hệ thống chỉ passive monitoring, không scan chủ động.
- Chỉ decode Ethernet với ARP và DHCP metadata cơ bản.
- DHCP hostname chỉ xuất hiện khi traffic có option tương ứng.
- Live capture cần quyền hệ thống và có thể khác nhau giữa Linux, macOS, Windows/Npcap, và Docker Desktop.
- PostgreSQL writer phụ thuộc client `psql` trong `PATH` và database đang reachable.
