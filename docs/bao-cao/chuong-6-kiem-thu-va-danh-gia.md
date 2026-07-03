# CHƯƠNG 6. KIỂM THỬ VÀ ĐÁNH GIÁ

## 6.1. Giới thiệu chương

Chương này trình bày quá trình kiểm thử và đánh giá hệ thống **Passive Network Asset Discovery System**. Mục tiêu của kiểm thử là xác nhận hệ thống đáp ứng các yêu cầu đã phân tích, hoạt động đúng với dữ liệu PCAP mẫu, xử lý được các lỗi phổ biến, ghi nhận được sự kiện, xuất kết quả đúng định dạng và có thể triển khai bằng Docker.

Do hệ thống làm việc với dữ liệu mạng, việc kiểm thử được chia thành hai nhóm chính. Nhóm thứ nhất là kiểm thử tự động bằng CTest, phù hợp với các thành phần logic và dữ liệu PCAP ổn định. Nhóm thứ hai là kiểm thử thủ công hoặc demo, phù hợp với live capture, Docker runtime và môi trường PostgreSQL thật.

## 6.2. Mục tiêu kiểm thử

Các mục tiêu kiểm thử chính gồm:

- Kiểm tra chương trình build thành công.
- Kiểm tra CLI xử lý đúng tham số hợp lệ và tham số lỗi.
- Kiểm tra parser đọc đúng Ethernet, ARP, DHCP, DNS và các giao thức liên quan.
- Kiểm tra asset store gom observation thành asset đúng quy tắc.
- Kiểm tra renderer xuất đúng table, JSON và CSV.
- Kiểm tra event detector phát hiện các sự kiện mạng quan trọng.
- Kiểm tra event log ghi đúng định dạng NDJSON.
- Kiểm tra PostgreSQL writer lưu dữ liệu đúng schema và không tạo trùng asset.
- Kiểm tra Docker image và Docker Compose chạy được.
- Đánh giá các hạn chế của phương pháp phát hiện thụ động.

## 6.3. Môi trường kiểm thử

Môi trường kiểm thử đề xuất:

| Thành phần | Phiên bản/ghi chú |
|---|---|
| Hệ điều hành | Linux hoặc môi trường hỗ trợ CMake và libpcap |
| Compiler | Trình biên dịch C++ hỗ trợ C++17 |
| Build system | CMake >= 3.16 |
| Packet capture | libpcap |
| Database | PostgreSQL 16 hoặc service trong Docker Compose |
| Container runtime | Docker Engine/Docker Desktop |
| Test runner | CTest |

**[PLACEHOLDER HÌNH 6.1: Chèn ảnh chụp màn hình kiểm tra phiên bản cmake, compiler, docker và psql trên máy kiểm thử]**

**Hình 6.1. Môi trường kiểm thử hệ thống**

## 6.4. Dữ liệu kiểm thử

Dữ liệu kiểm thử chính nằm trong thư mục `samples/` và một số fixture được sinh trong quá trình chạy test.

| Dữ liệu | Mục đích |
|---|---|
| `samples/arp.pcap` | Kiểm thử phát hiện asset từ ARP |
| `samples/multi-asset.pcap` | Kiểm thử nhiều thiết bị, nhiều IP, ARP và DHCP |
| PCAP DHCP sinh bởi test | Kiểm thử hostname DHCP |
| PCAP mac-change sinh bởi test | Kiểm thử event đổi MAC/IP |
| Empty PCAP fixture | Kiểm thử trường hợp không có asset |
| `generated-5m-arp.pcap` | Phục vụ kiểm tra tải hoặc benchmark |

PCAP là dữ liệu kiểm thử phù hợp vì ổn định, có thể chạy lại nhiều lần và không phụ thuộc traffic thực tế tại thời điểm demo.

## 6.5. Quy trình build và chạy test tự động

Các lệnh build và test cơ bản:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Khi cần kiểm thử đầy đủ tính năng PCAP, nên yêu cầu bắt buộc có libpcap:

```bash
cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

**[PLACEHOLDER HÌNH 6.2: Chèn ảnh chụp màn hình kết quả chạy ctest --test-dir build --output-on-failure, ưu tiên phần tổng kết 100% tests passed]**

**Hình 6.2. Kết quả chạy bộ kiểm thử tự động bằng CTest**

## 6.6. Kiểm thử đơn vị

Kiểm thử đơn vị tập trung vào các module độc lập. Các nhóm test chính gồm:

| Nhóm test | Mục đích |
|---|---|
| `TableRendererTests` | Kiểm tra output dạng bảng |
| `JsonRendererTests` | Kiểm tra output JSON |
| `CsvRendererTests` | Kiểm tra output CSV |
| `EthernetFrameTests` | Kiểm tra decode Ethernet frame |
| `ArpPacketTests` | Kiểm tra phân tích ARP |
| `PacketContextTests` | Kiểm tra xây dựng context từ packet |
| `ParserEngineTests` | Kiểm tra cơ chế match/parse plugin |
| `DnsPluginTests` | Kiểm tra DNS plugin |
| `AssetStoreTests` | Kiểm tra gom observation thành asset |
| `AssetEventDetectorTests` | Kiểm tra phát hiện event |
| `EventRateLimiterTests` | Kiểm tra giới hạn event trùng lặp |
| `AssetMonitorTests` | Kiểm tra phối hợp event detector và asset store |
| `ArgumentsTests` | Kiểm tra parse CLI |
| `AppConfigTests` | Kiểm tra merge và validate cấu hình |
| `CaptureBackendTests` | Kiểm tra capture backend |
| `BoundedQueueTests` | Kiểm tra hàng đợi giới hạn |
| `LiveCapturePipelineTests` | Kiểm tra pipeline live |
| `PostgresWriterTests` | Kiểm tra writer PostgreSQL |

Các test này giúp đảm bảo từng phần của hệ thống hoạt động đúng trước khi kiểm thử tích hợp.

## 6.7. Kiểm thử tích hợp với PCAP

### 6.7.1. Kiểm thử file ARP đơn giản

Lệnh kiểm thử:

```bash
./build/asset-discovery --pcap samples/arp.pcap --output table
```

Kết quả kỳ vọng:

- Phát hiện một asset.
- MAC: `02:42:ac:11:00:02`.
- IP: `192.168.1.10`.
- Nguồn phát hiện: `arp`.
- Hostname trống vì ARP không chứa hostname.

**[PLACEHOLDER HÌNH 6.3: Chèn ảnh chụp output table khi chạy samples/arp.pcap]**

**Hình 6.3. Kết quả kiểm thử phát hiện asset từ file ARP PCAP**

### 6.7.2. Kiểm thử output JSON

Lệnh kiểm thử:

```bash
./build/asset-discovery --pcap samples/arp.pcap --output json
```

Kết quả kỳ vọng là JSON array có field `mac_address`, `ip_addresses`, `first_seen`, `last_seen` và `discovery_sources`. JSON cần hợp lệ để có thể parse bằng script.

### 6.7.3. Kiểm thử output CSV

Lệnh kiểm thử:

```bash
./build/asset-discovery --pcap samples/arp.pcap --output csv
```

Kết quả kỳ vọng có header:

```csv
mac_address,ip_addresses,hostname,first_seen,last_seen,discovery_sources
```

CSV cần escape đúng với field chứa dấu phẩy, dấu quote hoặc xuống dòng.

### 6.7.4. Kiểm thử PCAP nhiều asset

Lệnh kiểm thử:

```bash
./build/asset-discovery --pcap samples/multi-asset.pcap --output table
```

Kết quả kỳ vọng:

- Phát hiện nhiều asset.
- Có thiết bị chỉ phát hiện qua ARP.
- Có thiết bị phát hiện qua cả ARP và DHCP.
- Có thiết bị có hostname từ DHCP như `laptop-user` hoặc `camera-01`.
- Có thiết bị có nhiều IP trong danh sách `ip_addresses`.

**[PLACEHOLDER HÌNH 6.4: Chèn ảnh chụp output table khi chạy samples/multi-asset.pcap]**

**Hình 6.4. Kết quả kiểm thử với PCAP nhiều tài sản mạng**

## 6.8. Kiểm thử BPF filter

BPF filter được kiểm thử để đảm bảo hệ thống chỉ xử lý nhóm packet mong muốn.

Chỉ phân tích ARP:

```bash
./build/asset-discovery --pcap samples/multi-asset.pcap --filter "arp" --output table
```

Kết quả kỳ vọng:

- Chỉ các asset xuất hiện qua ARP được hiển thị.
- Thiết bị chỉ có DHCP không xuất hiện.
- `discovery_sources` không chứa `dhcp` nếu packet DHCP bị filter loại bỏ.

Phân tích ARP và DHCP:

```bash
./build/asset-discovery --pcap samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Kết quả kỳ vọng là đầy đủ asset từ cả ARP và DHCP.

## 6.9. Kiểm thử event logging

Event logging được kiểm thử bằng PCAP có tình huống thay đổi MAC/IP hoặc thông tin hostname.

Lệnh kiểm thử:

```bash
rm -f logs/events.ndjson
./build/asset-discovery --pcap samples/multi-asset.pcap --output table
cat logs/events.ndjson
```

Kết quả kỳ vọng:

- File `logs/events.ndjson` được tạo.
- Mỗi dòng là một JSON object.
- Có event `new_asset` cho thiết bị mới.
- Có thể có event `ip_changed_for_mac`, `hostname_learned` hoặc event khác tùy dữ liệu PCAP.

**[PLACEHOLDER HÌNH 6.5: Chèn ảnh chụp nội dung logs/events.ndjson sau khi chạy demo]**

**Hình 6.5. Kết quả ghi sự kiện mạng ở định dạng NDJSON**

## 6.10. Kiểm thử PostgreSQL

### 6.10.1. Khởi động database

Khởi động PostgreSQL bằng Docker Compose:

```bash
docker compose up -d db
```

Schema được mount từ `db/schema.sql` để tạo bảng `assets` và `asset_events`.

### 6.10.2. Chạy chương trình và ghi database

Thiết lập biến môi trường hoặc `.env`:

```env
PGHOST=localhost
PGPORT=5432
PGDATABASE=asset_discovery
PGUSER=postgres
PGPASSWORD=123456
```

Chạy chương trình:

```bash
./build/asset-discovery --pcap samples/arp.pcap --output json
```

Truy vấn bảng `assets`:

```bash
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets;"
```

Kết quả kỳ vọng:

- Có bản ghi asset với MAC `02:42:ac:11:00:02`.
- IP `192.168.1.10` nằm trong `ip_addresses`.
- `discovery_sources` chứa `arp`.
- Chạy lại cùng PCAP không tạo thêm bản ghi trùng MAC.

**[PLACEHOLDER HÌNH 6.6: Chèn ảnh chụp kết quả truy vấn bảng assets trong PostgreSQL]**

**Hình 6.6. Kết quả lưu asset snapshot vào PostgreSQL**

### 6.10.3. Kiểm tra bảng sự kiện

Truy vấn bảng `asset_events`:

```bash
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select event_time, event_type, severity, ip_address, mac_address, protocol, message from asset_events order by id;"
```

Kết quả kỳ vọng:

- Có event tương ứng với quá trình phát hiện tài sản.
- Các field event type, severity, IP, MAC và protocol được ghi đúng.

## 6.11. Kiểm thử Docker

### 6.11.1. Build Docker image

Lệnh kiểm thử:

```bash
docker build -t passive-asset-discovery:latest .
```

Kết quả kỳ vọng:

- Image build thành công.
- Binary `asset-discovery` có trong runtime image.
- Image có libpcap runtime và PostgreSQL client.

### 6.11.2. Chạy PCAP trong container

Lệnh kiểm thử:

```bash
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  passive-asset-discovery:latest \
  --pcap /samples/arp.pcap \
  --output table
```

Kết quả kỳ vọng tương tự chạy native với `samples/arp.pcap`.

### 6.11.3. Chạy Docker Compose demo

Lệnh kiểm thử:

```bash
docker compose up --build pcap-demo
```

Kết quả kỳ vọng:

- Service `db` healthy.
- Service `pcap-demo` chạy phân tích `multi-asset.pcap`.
- Dữ liệu được ghi vào PostgreSQL service.
- Có thể dùng `docker compose exec db psql ...` để kiểm tra dữ liệu.

**[PLACEHOLDER HÌNH 6.7: Chèn ảnh chụp docker compose up --build pcap-demo chạy thành công]**

**Hình 6.7. Kết quả kiểm thử triển khai bằng Docker Compose**

## 6.12. Kiểm thử live capture

Live capture phụ thuộc vào hệ điều hành, quyền, interface và traffic thật, nên thường được kiểm thử thủ công.

Lệnh chạy:

```bash
sudo ./build/asset-discovery --profile live --interface eth0
```

Trong terminal khác, có thể sinh traffic bằng các thao tác như ping gateway, renew DHCP hoặc tạo ARP traffic. Khi có packet phù hợp, hệ thống cần in event và cập nhật summary khi dừng bằng `Ctrl+C`.

Kết quả kỳ vọng:

- Chương trình mở được interface.
- Packet được capture và parse.
- Event được ghi realtime.
- Khi nhấn `Ctrl+C`, chương trình dừng graceful.
- Summary cuối được in theo output format đã cấu hình.
- Metrics live được ghi ra stderr nếu có.

**[PLACEHOLDER HÌNH 6.8: Chèn ảnh chụp hai terminal khi chạy live capture: một terminal chạy asset-discovery, một terminal sinh traffic mạng]**

**Hình 6.8. Kiểm thử live capture trên network interface**

## 6.13. Kiểm thử xử lý lỗi

Các tình huống lỗi cần kiểm thử:

| Trường hợp | Lệnh ví dụ | Kết quả kỳ vọng |
|---|---|---|
| Thiếu input | `./build/asset-discovery` | Chương trình báo lỗi và exit khác 0 |
| File không tồn tại | `./build/asset-discovery --pcap samples/missing.pcap` | Báo lỗi không mở được file |
| Filter sai | `./build/asset-discovery --pcap samples/arp.pcap --filter "arp or"` | Báo lỗi BPF |
| Truyền hai nguồn input | `./build/asset-discovery --pcap a.pcap --interface eth0` | Báo lỗi chỉ được chọn một nguồn |
| Cờ cũ | `./build/asset-discovery --duration 10` | Báo cờ đã bị loại bỏ hoặc hướng dẫn thay thế |
| Thiếu quyền live | `./build/asset-discovery --interface eth0` | Báo lỗi quyền nếu không đủ quyền |
| Thiếu database config | Chạy khi không có `.env`/env DB | Báo thiếu cấu hình PostgreSQL nếu runtime yêu cầu |

**[PLACEHOLDER HÌNH 6.9: Chèn ảnh chụp một số thông báo lỗi tiêu biểu như thiếu input, file không tồn tại hoặc filter sai]**

**Hình 6.9. Kiểm thử xử lý lỗi và thông báo cho người dùng**

## 6.14. Đánh giá hiệu năng

Hệ thống có executable benchmark riêng:

```bash
./build-release/asset-discovery-live-benchmark --pcap samples/generated-5m-arp.pcap \
  --workers 7 \
  --batch-size 256 \
  --filter "arp or udp port 67 or udp port 68"
```

Đối với live backend trên Linux:

```bash
sudo ./build-release/asset-discovery-live-benchmark \
  --interface eth0 \
  --duration 30 \
  --backend af-packet \
  --workers 7 \
  --batch-size 256 \
  --filter "arp or udp port 67 or udp port 68"
```

Các chỉ số cần ghi nhận:

- Số packet captured.
- Số packet parsed.
- Số observation produced/applied.
- Số event produced/enqueued/dropped.
- Số batch bị drop do queue đầy.
- Throughput packet/giây.
- Backend được chọn.
- Kernel drops nếu backend hỗ trợ.

**[PLACEHOLDER HÌNH 6.10: Chèn ảnh chụp kết quả benchmark live pipeline hoặc offline benchmark, làm rõ throughput và drop counters]**

**Hình 6.10. Kết quả benchmark hiệu năng xử lý packet**

## 6.15. Đánh giá kết quả đạt được

Qua các kịch bản kiểm thử, hệ thống đạt được các kết quả chính:

- Phát hiện được tài sản mạng từ gói ARP.
- Tổng hợp được nhiều observation thành cùng một asset theo MAC.
- Trích xuất được hostname từ DHCP khi packet có dữ liệu phù hợp.
- Xuất được kết quả ở table, JSON và CSV.
- Ghi được sự kiện phát hiện tài sản và thay đổi trạng thái.
- Lưu được asset snapshot và event history vào PostgreSQL.
- Chạy được ở cả chế độ PCAP offline và live capture.
- Có khả năng đóng gói và demo bằng Docker.
- Có bộ test tự động bao phủ nhiều module quan trọng.

## 6.16. Hạn chế trong quá trình đánh giá

Hệ thống vẫn có một số hạn chế:

- Phương pháp thụ động chỉ phát hiện thiết bị có phát sinh traffic trong thời gian quan sát.
- Thiết bị im lặng hoặc đang tắt có thể không được phát hiện.
- Hostname phụ thuộc vào giao thức như DHCP, NBNS hoặc các gói tin có chứa tên thiết bị.
- Live capture phụ thuộc quyền hệ thống và môi trường mạng thật.
- Docker live capture trên macOS/Windows có thể bị giới hạn do Docker Desktop không tương đương host networking Linux.
- PostgreSQL writer hiện phụ thuộc `psql` trong `PATH`.
- Dữ liệu PCAP có thể chứa thông tin nhạy cảm, cần quản lý cẩn thận khi dùng trong demo.

## 6.17. Tổng kết chương

Chương này đã trình bày các hoạt động kiểm thử và đánh giá hệ thống Passive Network Asset Discovery System. Hệ thống được kiểm thử ở nhiều cấp độ, từ unit test, integration test với PCAP, kiểm thử event log, kiểm thử PostgreSQL, Docker và live capture.

Kết quả kiểm thử cho thấy hệ thống đáp ứng được các yêu cầu chính của đề tài: phát hiện tài sản mạng bằng phương pháp thụ động, tổng hợp thông tin thiết bị, xuất kết quả ở nhiều định dạng, ghi sự kiện và triển khai được bằng Docker. Các hạn chế được ghi nhận là cơ sở để đề xuất hướng phát triển trong chương tiếp theo.
