# ĐỀ TÀI 1: Passive Network Asset Discovery System

## Mô tả đề tài

Đề tài tập trung xây dựng một hệ thống giám sát mạng thụ động (passive monitoring system) có khả năng phân tích lưu lượng mạng nhằm phát hiện các thiết bị hoạt động trong mạng nội bộ và thu thập một số thông tin cơ bản của thiết bị.

Hệ thống cần xử lý traffic từ network interface hoặc file PCAP để:
- nhận diện asset xuất hiện trong mạng thông qua địa chỉ IP/MAC,
- theo dõi thời điểm asset bắt đầu và ngừng xuất hiện,
- phân tích một số protocol discovery cơ bản như ARP và DHCP nhằm thu thập metadata liên quan đến asset.

Toàn bộ hệ thống cần được đóng gói và triển khai bằng Docker.

Sinh viên được tự do lựa chọn kiến trúc hệ thống, công nghệ sử dụng và phương pháp xử lý packet.

---

# Yêu cầu đầu ra

Hệ thống tối thiểu cần:
- phát hiện asset mới từ traffic mạng,
- thu thập một số thông tin cơ bản của asset từ ARP/DHCP traffic (ví dụ: IP, MAC, first seen, last seen, hostname, ...),
- hoạt động được trên traffic thực tế hoặc file PCAP,
- triển khai được bằng Docker.

---

# Kết quả mong đợi

Sinh viên cần cung cấp:
- source code hoàn chỉnh,
- tài liệu mô tả thiết kế hệ thống,
- hướng dẫn build/deploy,
- Dockerfile hoặc docker-compose,
- demo hoạt động của hệ thống.

---

# Cách build và kiểm thử hiện tại

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

# Ví dụ chạy

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

Lọc packet bằng BPF để chỉ xử lý ARP/DHCP:

```sh
./build/asset-discovery --pcap samples/arp.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Live capture từ interface, chạy tới khi nhấn Ctrl+C hoặc nhận SIGTERM:

```sh
sudo ./build/asset-discovery --interface eth0
```

Chạy với config/profile để không phải truyền lại các tham số policy:

```sh
sudo ./build/asset-discovery --profile live --interface eth0
./build/asset-discovery --config configs/pcap.yaml --pcap samples/arp.pcap
```

Thứ tự ưu tiên cấu hình:

```text
Built-in defaults
  -> configs/default.yaml nếu tồn tại
  -> --config <file> hoặc --profile <name>
  -> biến môi trường
  -> CLI arguments
```

`configs/*.yaml` chỉ cấu hình policy/runtime như `capture.filter`, `capture.backend`, `output.format`, `events.*`, `network.local_nets`, và `network.ignore_nets`. Nguồn packet vẫn phải truyền rõ trên CLI bằng `--pcap` hoặc `--interface`. Database URL và event sink path vẫn đi qua `.env`/environment, không đặt trong YAML.

Ghi PostgreSQL bằng database từ Docker Compose khi `psql` có trong `PATH`:

```sh
docker compose up -d db
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" -f db/schema.sql
./build/asset-discovery --pcap samples/arp.pcap --output json
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets;"
```

Tạo `.env` trong thư mục hiện tại:

```env
PGHOST=localhost
PGPORT=5432
PGDATABASE=asset_discovery
PGUSER=postgres
PGPASSWORD=123456
```

Sau đó chỉ cần:

```sh
./build/asset-discovery --pcap samples/arp.pcap --output json
```

Chương trình yêu cầu cấu hình PostgreSQL trước khi capture để lưu asset snapshot và event history. Trong test/demo không có PostgreSQL thật, có thể dùng stub `psql` như các test trong `tests/` đang làm.

## Docker

Build image:

```sh
docker build -t passive-asset-discovery .
```

Chạy PCAP bằng Docker:

```sh
docker run --rm -v "$PWD/samples:/samples:ro" passive-asset-discovery \
  --pcap /samples/arp.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Chạy PCAP cùng PostgreSQL bằng Docker Compose:

```sh
docker compose up --build pcap-demo
docker compose exec pcap-demo ping db
docker compose exec db psql -U postgres -d asset_discovery \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets;"
```

Sau khi đọc xong PCAP, service `pcap-demo` vẫn giữ container sống để có thể kiểm tra DNS hoặc database bằng `docker compose exec`.

Nếu muốn chạy binary local nhưng dùng PostgreSQL từ Docker Compose:

```sh
docker compose up -d db
./build/asset-discovery --pcap samples/arp.pcap --output json
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets;"
```

Compose publish PostgreSQL trên port host mặc định `5432`. Nếu máy đã có PostgreSQL local dùng port này, đặt `POSTGRES_HOST_PORT=<port-khac>` khi chạy `docker compose up -d db`.

Nếu volume `postgres-data` đã từng được tạo với credential khác, PostgreSQL sẽ giữ credential cũ. Khi chỉ dùng dữ liệu demo và chấp nhận xóa database cũ, chạy `docker compose down -v` rồi tạo lại service `db`.

Live capture trong Docker cần chạy trên Linux host có interface phù hợp và cấp quyền capture:

```sh
docker run --rm --user 0:0 --net=host --cap-add=NET_ADMIN --cap-add=NET_RAW \
  passive-asset-discovery \
  --profile live --interface eth0
```

Có thể dùng Compose profile cho live capture:

```sh
CAPTURE_INTERFACE=eth0 docker compose --profile live run --rm live-capture
```

Tài liệu Sprint, thiết kế và hướng dẫn được tổ chức lại như sau:

* **Sprints Notes:**
  * [docs/sprints/SPRINT2.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/sprints/SPRINT2.md)
  * [docs/sprints/SPRINT3.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/sprints/SPRINT3.md)
  * [docs/sprints/SPRINT4.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/sprints/SPRINT4.md)
* **Thiết kế & Đặc tả (Design & Spec):**
  * [docs/design-spec/system-design.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/design-spec/system-design.md)
  * [docs/design-spec/asset-events.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/design-spec/asset-events.md)
  * [docs/design-spec/asset-output-contract.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/design-spec/asset-output-contract.md)
  * [docs/design-spec/system-programming-improvement-proposal.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/design-spec/system-programming-improvement-proposal.md)
  * [docs/design-spec/docker-packaging-guide.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/design-spec/docker-packaging-guide.md)
* **Hướng dẫn Demo:**
  * [docs/build-deploy-demo-guide.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/build-deploy-demo-guide.md) (Mục lục dẫn hướng chi tiết)
* **Checklist Bàn giao:**
  * [docs/final-submission-checklist.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/final-submission-checklist.md)

---

## Giám sát Sự kiện Mạng (Asset Event Logging)

Hệ thống tích hợp bộ phát hiện sự kiện tương tự `arpwatch` nhằm giám sát và cảnh báo các hành vi mạng theo thời gian thực (được bật mặc định).

### Các loại sự kiện hỗ trợ
- `new_asset`: Phát hiện MAC mới xuất hiện lần đầu.
- `mac_changed_for_ip`: IP bị đổi sang MAC khác (cảnh báo IP Conflict hoặc ARP spoofing).
- `ip_changed_for_mac`: MAC gán với IP mới.
- `ip_mac_flip_flop`: Hiện tượng đổi IP-MAC qua lại liên tục trong thời gian ngắn.
- `hostname_learned`/`hostname_changed`: Nhận diện và phát hiện thay đổi hostname thiết bị.
- `non_local_source_ip`: IP không thuộc dải mạng cục bộ (ngoại mạng).
- `ethernet_arp_mac_mismatch`: MAC lớp Ethernet khác với MAC trong gói tin ARP.

### Cấu hình và Đầu ra
Mặc định, các sự kiện mạng được ghi song song ra nhiều kênh đầu ra (sinks):
1. **Terminal Console (stdout)**: Hiển thị các sự kiện phát hiện được dưới dạng text dễ đọc.
2. **File NDJSON**: Tự động lưu trữ dưới định dạng JSON mỗi dòng một log tại `logs/events.ndjson`. Đường dẫn này có thể ghi đè qua biến môi trường `ASSET_DISCOVERY_EVENTS_JSON`.
3. **Cơ sở dữ liệu (PostgreSQL)**: Nếu biến môi trường PostgreSQL hoạt động, các sự kiện sẽ tự động ghi vào bảng `asset_events`.

### Cấu hình policy sự kiện
Nên cấu hình các ngưỡng phát hiện sự kiện trong `configs/default.yaml`, `configs/live.yaml`, hoặc file truyền bằng `--config`. CLI vẫn hỗ trợ các override nâng cao khi cần test nhanh:
- `--local-net <cidr>`: Cấu hình dải mạng cục bộ (ví dụ: `192.168.1.0/24`).
- `--ignore-net <cidr>`: Bỏ qua kiểm tra đối với dải mạng tin cậy.
- `--event-rate-limit <sec>`: Suppress event trùng/lặp trong khoảng thời gian này.
- `--flip-flop-window <seconds>`: Cửa sổ thời gian xác định hành vi flip-flop.
- `--reappearance-threshold <seconds>`: Thời gian không hoạt động trước khi coi thiết bị tái xuất hiện (`asset_reappeared`).

### Ví dụ kiểm tra dữ liệu sự kiện trong PostgreSQL

```sh
# Khởi động PostgreSQL và nạp schema
docker compose up -d db
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" -f db/schema.sql

# Chạy hệ thống với file PCAP mẫu có hành vi đổi MAC (mac_changed_for_ip)
./build/asset-discovery --pcap samples/arp.pcap

# Truy vấn bảng lịch sử sự kiện mạng
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select event_time, event_type, severity, ip_address, mac_address, old_mac, new_mac, message from asset_events order by id;"
```

---

## Cấu trúc Thư mục Tài liệu

Tài liệu Sprint, thiết kế, hướng dẫn demo và checklist bàn giao được tổ chức như sau:

* **Sprints Notes:**
  * [docs/sprints/SPRINT2.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/sprints/SPRINT2.md)
  * [docs/sprints/SPRINT3.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/sprints/SPRINT3.md)
  * [docs/sprints/SPRINT4.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/sprints/SPRINT4.md)
* **Thiết kế & Đặc tả (Design & Spec):**
  * [docs/design-spec/system-design.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/design-spec/system-design.md)
  * [docs/design-spec/asset-events.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/design-spec/asset-events.md)
  * [docs/design-spec/asset-output-contract.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/design-spec/asset-output-contract.md)
  * [docs/design-spec/system-programming-improvement-proposal.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/design-spec/system-programming-improvement-proposal.md)
  * [docs/design-spec/docker-packaging-guide.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/design-spec/docker-packaging-guide.md)
* **Hướng dẫn Demo (13 Phần Chi Tiết):**
  * [docs/build-deploy-demo-guide.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/build-deploy-demo-guide.md) (Mục lục dẫn hướng chi tiết)
  * [Phần 1: Chuẩn Bị Trước Demo](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/01-preparation.md)
  * [Phần 2: Build Native Và Chạy Test](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/02-build-and-test.md)
  * [Phần 3: Demo PCAP Offline](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/03-pcap-offline.md)
  * [Phần 4: PostgreSQL Local & Tích Hợp](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/04-postgres-local.md)
  * [Phần 5: Demo Đóng Gói Docker Image](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/05-docker-image.md)
  * [Phần 6: Demo Triển Khai Với Docker Compose](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/06-docker-compose.md)
  * [Phần 7: Demo Live Capture Native](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/07-live-capture-native.md)
  * [Phần 8: Demo Live Capture Trong Docker](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/08-live-capture-docker.md)
  * [Phần 9: Demo Xử Lý Lỗi & Xác Thực](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/09-error-handling.md)
  * [Phần 10: Demo CLI Help Menu](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/10-help-text.md)
  * [Phần 11: Checklist Bằng Chứng Demo](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/11-evidence-checklist.md)
  * [Phần 12: Hướng Dẫn Giải Quyết Sự Cố](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/12-troubleshooting.md)
  * [Phần 13: Tham Số CLI, Config Và Giá Trị Mặc Định](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/demo/13-cli-parameters.md)
* **Checklist Bàn giao:**
  * [docs/final-submission-checklist.md](file:///home/nhani05/vdt/passive-network-asset-discovery-system/docs/final-submission-checklist.md)
