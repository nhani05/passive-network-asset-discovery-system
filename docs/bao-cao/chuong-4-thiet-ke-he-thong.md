# CHƯƠNG 4. THIẾT KẾ HỆ THỐNG

## 4.1. Giới thiệu chương

Chương này trình bày thiết kế của hệ thống **Passive Network Asset Discovery System** dựa trên các yêu cầu đã phân tích ở Chương 3. Mục tiêu thiết kế là xây dựng một hệ thống có khả năng phân tích lưu lượng mạng thụ động, hỗ trợ cả file PCAP và live capture, có kiến trúc module rõ ràng, dễ kiểm thử và có khả năng mở rộng thêm giao thức trong tương lai.

Hệ thống được triển khai bằng C++17, build bằng CMake và tổ chức theo nhiều module độc lập. Mỗi module đảm nhiệm một nhóm trách nhiệm riêng như xử lý CLI, load cấu hình, capture packet, phân tích gói tin, tổng hợp tài sản, phát hiện sự kiện, render output và ghi database. Cách chia này giúp giảm phụ thuộc giữa các thành phần và hỗ trợ kiểm thử từng phần.

## 4.2. Mục tiêu thiết kế

Thiết kế hệ thống hướng đến các mục tiêu sau:

- **Thụ động:** hệ thống chỉ lắng nghe hoặc đọc lưu lượng có sẵn, không gửi packet thăm dò.
- **Tách biệt trách nhiệm:** mỗi module xử lý một nhiệm vụ rõ ràng.
- **Dễ mở rộng parser:** có thể thêm giao thức mới bằng plugin mà không thay đổi toàn bộ pipeline.
- **Đầu ra linh hoạt:** hỗ trợ table, JSON, CSV, NDJSON event log và PostgreSQL.
- **An toàn khi xử lý packet:** kiểm tra độ dài buffer trước khi đọc field.
- **Phù hợp live capture:** tách capture, parse, aggregation và event writing bằng pipeline có queue giới hạn.
- **Dễ triển khai:** hỗ trợ build native, Docker image và Docker Compose.

## 4.3. Kiến trúc tổng thể

Ở mức tổng thể, hệ thống gồm các lớp chính:

| Lớp | Thành phần | Trách nhiệm |
|---|---|---|
| Interface | CLI | Nhận tham số chạy, chọn nguồn packet, output format, config/profile |
| Application | Config, Live Pipeline | Load cấu hình, điều phối live capture và xử lý runtime |
| Capture | PCAP, AF_PACKET/libpcap | Đọc packet từ file hoặc interface |
| Packet Processing | Parser Core, Parser Plugins | Decode packet và sinh observation |
| Discovery | AssetStore, AssetMonitor | Gom observation thành asset và phát hiện sự kiện |
| Event | EventDetector, EventSink | Tạo và ghi event ra stdout, NDJSON, syslog, database |
| Output | Table, JSON, CSV Renderer | Xuất summary cuối |
| Storage | PostgreSQL Writer | Lưu asset snapshot và event history |

**[PLACEHOLDER HÌNH 4.1: Chèn sơ đồ kiến trúc tổng thể theo lớp, từ CLI/Config đến Capture, Parser, AssetMonitor, Output và Storage]**

**Hình 4.1. Kiến trúc tổng thể của hệ thống Passive Network Asset Discovery**

## 4.4. Luồng dữ liệu tổng quát

Luồng dữ liệu của hệ thống bắt đầu từ nguồn packet. Nguồn này có thể là file PCAP hoặc network interface. Packet sau khi được capture sẽ được chuyển thành dữ liệu đầu vào cho parser. Parser phân tích Ethernet frame, xây dựng `PacketContext`, sau đó các parser plugin kiểm tra và trích xuất thông tin phù hợp.

Kết quả của parser là các `AssetObservation`. Các observation này được gửi đến `AssetMonitor`. `AssetMonitor` sử dụng `AssetEventDetector` để phát hiện sự kiện, sau đó cập nhật `AssetStore`. Cuối cùng, danh sách asset được render ra output và ghi vào PostgreSQL nếu cấu hình database hợp lệ.

```text
PCAP file / Network interface
        |
        v
PacketCaptureBackend
        |
        v
OfflinePacket / PacketView
        |
        v
PacketContext
        |
        v
ParserEngine + ParserRegistry
        |
        v
AssetObservation
        |
        v
AssetMonitor
        |
        +--> AssetEventDetector --> EventSink
        |
        v
AssetStore
        |
        +--> Table / JSON / CSV Renderer
        |
        +--> PostgreSQL Writer
```

**[PLACEHOLDER HÌNH 4.2: Chèn data flow diagram thể hiện luồng PCAP/Interface → Capture → Parser → Observation → AssetMonitor → AssetStore → Renderer/PostgreSQL/EventSink]**

**Hình 4.2. Luồng dữ liệu tổng quát của hệ thống**

## 4.5. Thiết kế module CLI

Module CLI chịu trách nhiệm phân tích tham số dòng lệnh. Các tham số chính gồm:

- `--pcap <file>`: chạy chế độ đọc file PCAP.
- `--interface <name>`: chạy chế độ live capture.
- `--config <file>`: load file cấu hình cụ thể.
- `--profile <name>`: load profile trong thư mục `configs`.
- `--filter <bpf>`: cấu hình biểu thức BPF.
- `--capture-backend <auto|pcap|af-packet>`: chọn backend capture.
- `--output <table|json|csv>`: chọn định dạng summary cuối.
- `--version`: in phiên bản.
- `--help`: in hướng dẫn sử dụng.

Thiết kế CLI yêu cầu người dùng phải chọn đúng một nguồn packet. Các cờ cũ như `--duration`, `--live`, `--idle-timeout`, `--max-assets`, `--db-url`, `--events-json` được từ chối với thông báo migration rõ ràng. Cách làm này giúp tránh việc người dùng chạy sai theo interface cũ.

## 4.6. Thiết kế cấu hình ứng dụng

Module cấu hình có nhiệm vụ hợp nhất nhiều nguồn cấu hình thành một `AppConfig` đã validate. Thứ tự ưu tiên như sau:

```text
Built-in defaults
  -> configs/default.yaml
  -> --config <file> hoặc --profile <name>
  -> .env / process environment
  -> CLI arguments
```

Các giá trị chính trong YAML gồm:

- `capture.filter`
- `capture.backend`
- `output.format`
- `events.rate_limit_sec`
- `events.queue_capacity`
- `events.flip_flop_window_sec`
- `events.reappearance_threshold_sec`
- `network.local_nets`
- `network.ignore_nets`

Nguồn packet không được đặt trong YAML. Người dùng vẫn phải chọn `--pcap` hoặc `--interface` trên CLI để tránh chạy nhầm nguồn dữ liệu. Database URL, user, password và event file path được cấu hình qua `.env` hoặc biến môi trường.

## 4.7. Thiết kế capture layer

Capture layer cung cấp khả năng đọc packet từ hai nguồn:

- **PCAP offline:** đọc gói tin từ file PCAP.
- **Live interface:** bắt gói tin trực tiếp từ interface mạng.

Đối với live capture, hệ thống hỗ trợ backend `pcap`, `af-packet` hoặc `auto`. Backend `pcap` sử dụng libpcap, có tính portable tốt hơn. Backend `af-packet` sử dụng cơ chế packet socket của Linux, phù hợp cho môi trường Linux cần hiệu năng cao hơn và có quyền `CAP_NET_RAW`.

Backend capture chỉ nhận Ethernet datalink trong phạm vi hiện tại. BPF filter được compile bằng libpcap trước khi xử lý packet. Với backend AF_PACKET, filter có thể được attach vào socket bằng cơ chế classic BPF.

## 4.8. Thiết kế packet parser

Packet parser được chia thành parser core và parser plugin. Parser core chịu trách nhiệm xây dựng `PacketContext` từ bytes thô của packet. `PacketContext` chứa các thông tin đã decode như Ethernet, IPv4, UDP và payload nếu hợp lệ.

Parser plugin triển khai hai bước:

- `match(PacketContext)`: kiểm tra packet có phù hợp với plugin không.
- `parse(PacketContext)`: phân tích packet và sinh ra danh sách `AssetObservation`.

Các plugin built-in hiện có gồm:

- ARP plugin.
- DHCP plugin.
- DNS plugin.
- NetBIOS/NBNS plugin.
- SSDP plugin.
- TCP plugin.

**[PLACEHOLDER HÌNH 4.3: Chèn sơ đồ class/module cho ParserEngine, ParserRegistry, ParserInterface và các plugin ARP/DHCP/DNS/NBNS/SSDP/TCP]**

**Hình 4.3. Thiết kế parser core và parser plugin**

Thiết kế plugin giúp hệ thống dễ mở rộng. Khi cần hỗ trợ giao thức mới, có thể thêm plugin mới và đăng ký vào registry mà không phải sửa logic tổng hợp asset.

## 4.9. Thiết kế mô hình AssetObservation

`AssetObservation` là dữ liệu trung gian giữa parser và discovery layer. Một observation thể hiện một thông tin quan sát được từ một packet hoặc một giao thức.

Các trường thông tin chính gồm:

- `macAddress`: địa chỉ MAC quan sát được.
- `ipAddress`: địa chỉ IP quan sát được.
- `hostname`: hostname nếu có.
- `sourceId`: nguồn phát hiện, ví dụ `arp`, `dhcp`, `dns`.
- `eventType`: loại observation hoặc ngữ cảnh sự kiện.
- `confidence`: độ tin cậy tương đối.
- `metadata`: thông tin bổ sung dạng key-value.
- `timestamp`: thời điểm packet được quan sát.

Thiết kế observation giúp parser không cần biết cách asset được lưu trữ. Parser chỉ cần tạo thông tin quan sát, còn việc gom nhóm thuộc trách nhiệm của discovery layer.

## 4.10. Thiết kế AssetStore

`AssetStore` lưu trạng thái cuối của các asset đã phát hiện. Asset được định danh bằng địa chỉ MAC đã chuẩn hóa chữ thường. Một asset có thể có nhiều địa chỉ IP vì thiết bị có thể đổi IP hoặc xuất hiện trong nhiều packet khác nhau.

Quy tắc cập nhật asset:

- MAC mới tạo asset mới.
- Observation cùng MAC cập nhật `last_seen`.
- Timestamp sớm hơn cập nhật `first_seen`.
- IP mới được thêm vào `ip_addresses`.
- Hostname không rỗng cập nhật `hostname`.
- Source protocol được thêm vào `discovery_sources`.

**[PLACEHOLDER HÌNH 4.4: Chèn sơ đồ minh họa nhiều AssetObservation từ ARP/DHCP/DNS được gom vào một Asset trong AssetStore]**

**Hình 4.4. Cơ chế gom observation thành asset**

## 4.11. Thiết kế AssetMonitor và EventDetector

`AssetMonitor` là thành phần điều phối giữa event detection và asset store. Khi nhận observation, monitor thực hiện:

1. Gửi observation vào `AssetEventDetector` để kiểm tra sự kiện.
2. Phát event nếu có.
3. Cập nhật `AssetStore`.

`AssetEventDetector` giữ trạng thái cần thiết để phát hiện các thay đổi như IP đổi MAC, MAC đổi IP, hostname thay đổi hoặc thiết bị xuất hiện lại sau thời gian dài. Các event có severity khác nhau như `info`, `warning` hoặc `high`.

Cách tách event detector khỏi asset store giúp hệ thống vừa duy trì snapshot cuối, vừa ghi được timeline sự kiện. Snapshot trả lời câu hỏi "hiện biết những asset nào", còn event history trả lời câu hỏi "điều gì đã xảy ra trong quá trình quan sát".

## 4.12. Thiết kế event sink

Event sink chịu trách nhiệm ghi event ra các đích khác nhau:

- stdout dạng text dễ đọc.
- File NDJSON, mặc định `logs/events.ndjson`.
- Syslog nếu platform hỗ trợ.
- PostgreSQL bảng `asset_events` khi database được cấu hình.

Định dạng NDJSON được chọn vì mỗi dòng là một JSON object độc lập. Điều này giúp log dễ append, dễ đọc bằng script và phù hợp với các công cụ xử lý log.

## 4.13. Thiết kế output renderer

Hệ thống có ba renderer cho summary asset cuối:

- `TableRenderer`: phục vụ người dùng đọc trực tiếp trên terminal.
- `JsonRenderer`: phục vụ tích hợp máy đọc.
- `CsvRenderer`: phục vụ lưu trữ đơn giản hoặc mở bằng bảng tính.

Tất cả renderer dùng chung contract dữ liệu asset gồm `mac_address`, `ip_addresses`, `hostname`, `first_seen`, `last_seen` và `discovery_sources`. Việc thống nhất contract giúp output ổn định giữa các định dạng.

## 4.14. Thiết kế lưu trữ PostgreSQL

PostgreSQL được dùng để lưu trạng thái cuối của asset và lịch sử event. Database có hai bảng chính:

- `assets`: lưu snapshot cuối, khóa chính là `mac_address`.
- `asset_events`: lưu lịch sử event theo thời gian.

Schema rút gọn:

```sql
CREATE TABLE IF NOT EXISTS assets (
    mac_address TEXT PRIMARY KEY,
    ip_addresses TEXT[] NOT NULL DEFAULT '{}',
    hostname TEXT,
    first_seen TEXT NOT NULL,
    last_seen TEXT NOT NULL,
    discovery_sources TEXT[] NOT NULL DEFAULT '{}',
    observed_metadata JSONB NOT NULL DEFAULT '{}'::jsonb,
    reference_metadata JSONB NOT NULL DEFAULT '{}'::jsonb,
    derived_hints JSONB NOT NULL DEFAULT '[]'::jsonb,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

**[PLACEHOLDER HÌNH 4.5: Chèn ERD gồm bảng assets và asset_events, thể hiện các trường chính và quan hệ logic theo mac_address/ip_address]**

**Hình 4.5. Mô hình lưu trữ dữ liệu trong PostgreSQL**

Writer sử dụng cơ chế `INSERT ... ON CONFLICT (mac_address) DO UPDATE` để cập nhật asset nếu MAC đã tồn tại. Điều này giúp chạy lại cùng một file PCAP không tạo duplicate asset.

## 4.15. Thiết kế live capture pipeline

Ở chế độ PCAP, hệ thống có thể đọc tuần tự từng packet và xử lý đồng bộ. Tuy nhiên, ở chế độ live capture, nếu một bước xử lý chậm, capture có thể bị nghẽn. Vì vậy hệ thống thiết kế live pipeline theo mô hình producer-consumer.

Các thành phần trong live pipeline:

- Capture thread đọc packet từ interface.
- Bounded packet queue chứa batch packet.
- Parser worker pool xử lý packet song song.
- Bounded observation queue chứa batch observation.
- Aggregator thread cập nhật `AssetMonitor`.
- Bounded event queue chứa event.
- Event writer thread ghi event ra sink.

```text
Capture Thread
      |
      v
Bounded Packet Queue
      |
      v
Parser Worker Pool
      |
      v
Bounded Observation Queue
      |
      v
Aggregator Thread
      |
      v
Bounded Event Queue
      |
      v
Event Writer Thread
```

**[PLACEHOLDER HÌNH 4.6: Chèn sơ đồ live capture pipeline gồm Capture Thread, Packet Queue, Parser Workers, Observation Queue, Aggregator, Event Queue và Event Writer]**

**Hình 4.6. Thiết kế pipeline xử lý live capture**

Aggregator là single writer duy nhất của `AssetMonitor` và `AssetStore`. Thiết kế này giúp tránh lỗi race condition khi nhiều parser worker chạy song song. Parser worker chỉ tạo observation, không cập nhật asset store trực tiếp.

## 4.16. Thiết kế Docker và triển khai

Hệ thống sử dụng Dockerfile multi-stage:

- Stage build cài compiler, CMake, `pkg-config`, `libpcap-dev` và build binary.
- Stage runtime cài `libpcap0.8`, `postgresql-client`, copy binary và chạy bằng user `asset`.

Docker Compose gồm các service chính:

- `db`: PostgreSQL 16 Alpine, mount schema SQL để khởi tạo database.
- `pcap-demo`: build image, mount `samples`, chạy phân tích PCAP và ghi database.
- `live-capture`: chạy theo profile `live`, dùng host network và capability `NET_ADMIN`, `NET_RAW`.

**[PLACEHOLDER HÌNH 4.7: Chèn deployment diagram Docker Compose gồm db, pcap-demo, live-capture, volume postgres-data và network asset-net]**

**Hình 4.7. Thiết kế triển khai bằng Docker Compose**

## 4.17. Thiết kế xử lý lỗi

Hệ thống cần xử lý các lỗi phổ biến:

- Thiếu nguồn input.
- Truyền đồng thời `--pcap` và `--interface`.
- File PCAP không tồn tại.
- BPF filter sai cú pháp.
- Backend capture không khả dụng.
- Thiếu quyền live capture.
- Thiếu cấu hình PostgreSQL.
- Database không reachable.
- YAML config sai schema hoặc có key không hỗ trợ.

Thông báo lỗi cần in ra stderr và trả về exit code khác 0. Output JSON không nên bị lẫn warning không liên quan trên stdout để tránh làm hỏng dữ liệu máy đọc.

## 4.18. Khả năng mở rộng

Thiết kế hiện tại có thể mở rộng theo nhiều hướng:

- Thêm parser plugin cho mDNS, LLMNR hoặc giao thức IoT khác.
- Bổ sung enrichment từ OUI vendor database.
- Thêm REST API hoặc dashboard.
- Lưu thêm metadata thiết bị.
- Tích hợp hệ thống cảnh báo bên ngoài.
- Hỗ trợ xuất Prometheus metrics cho live mode.

Cấu trúc module và mô hình observation giúp các hướng mở rộng trên không cần thay đổi quá nhiều phần lõi.

## 4.19. Tổng kết chương

Chương này đã trình bày thiết kế hệ thống Passive Network Asset Discovery System. Hệ thống được chia thành các module rõ ràng từ CLI, config, capture, parser, discovery, event, output đến storage. Luồng dữ liệu được thiết kế thống nhất cho cả PCAP mode và live capture mode. Riêng live capture sử dụng pipeline đa luồng với queue giới hạn để tăng khả năng xử lý và kiểm soát bộ nhớ.

Thiết kế này là cơ sở cho phần triển khai cụ thể ở Chương 5, nơi trình bày công nghệ sử dụng, cấu trúc mã nguồn, cách build, cách chạy và các thành phần đã được hiện thực trong repository.
