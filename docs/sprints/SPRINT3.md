# Sprint 3: Live Capture Và Docker

Sprint 3 đưa hệ thống từ luồng PCAP offline sang môi trường demo/container có thể lặp lại, đồng thời hỗ trợ live capture có giới hạn thời gian.

## Mục Tiêu Sau Triển Khai

- Cho phép dùng cùng một pipeline parser/asset/output cho cả file PCAP và live capture từ network interface.
- Cho phép giới hạn traffic đầu vào bằng biểu thức BPF trước khi parser xử lý packet.
- Đóng gói binary C++17 trong Docker image có libpcap và `psql` runtime để demo PCAP và ghi PostgreSQL.
- Cung cấp Compose stack lặp lại được gồm PostgreSQL, demo PCAP, và profile live capture riêng.

## Phạm Vi

| Hạng mục | Trạng thái | Ghi chú |
| --- | --- | --- |
| Live capture | Hoàn tất | CLI hỗ trợ `--interface <name>` và `--duration <seconds>`. |
| BPF filter | Hoàn tất | CLI hỗ trợ `--filter <bpf>` cho cả PCAP và live capture. |
| Dockerfile | Hoàn tất | Image multi-stage build binary C++17 với libpcap và runtime có `psql`. |
| Docker Compose | Hoàn tất | `pcap-demo` chạy sample PCAP và ghi PostgreSQL; `live-capture` nằm sau profile `live`. |
| Tài liệu quyền Docker | Hoàn tất | Live capture cần host network và capability `NET_ADMIN`, `NET_RAW`. |

## Thay Đổi Chính

### CLI

`asset-discovery` hiện có hai chế độ input loại trừ nhau:

```text
asset-discovery --pcap <file> [--filter <bpf>] [--output table|json|csv] [--db-url <url>]
asset-discovery --interface <name> --duration <seconds> [--filter <bpf>] [--output table|json|csv] [--db-url <url>]
```

Quy tắc input:

- Phải chọn đúng một nguồn: `--pcap` hoặc `--interface`.
- Khi dùng `--interface`, `--duration <seconds>` là bắt buộc và phải là số nguyên dương.
- `--filter` là tùy chọn, nhưng nếu truyền thì giá trị không được rỗng.
- Nếu filter BPF sai cú pháp, chương trình thoát khác 0 và in lỗi tiếng Anh từ libpcap.

### Capture

- PCAP mode mở file bằng libpcap, kiểm tra datalink Ethernet, áp dụng BPF nếu có, rồi đọc toàn bộ packet.
- Live mode mở interface bằng libpcap với snaplen `65535`, promiscuous mode bật, timeout đọc packet `1000ms`, và dừng khi hết `--duration`.
- Packet không phải Ethernet hoặc không parse được vẫn bị bỏ qua an toàn ở pipeline parser hiện có.

### Docker

`Dockerfile` dùng multi-stage build:

- Stage build: `ubuntu:22.04`, `build-essential`, `cmake`, `pkg-config`, `libpcap-dev`, build với `ASSET_DISCOVERY_REQUIRE_PCAP=ON`.
- Stage runtime: `ubuntu:22.04`, `libpcap0.8`, `postgresql-client`, user không đặc quyền `asset`, entrypoint `asset-discovery`.

`docker-compose.yml` cung cấp:

- `db`: PostgreSQL 16 Alpine với database `asset_discovery`, user `postgres`, password `123456`, và port host mặc định `5432` qua biến `POSTGRES_HOST_PORT`.
- `pcap-demo`: build image, mount `./samples` read-only, chạy `/samples/arp.pcap`, áp dụng filter ARP/DHCP, ghi PostgreSQL qua `DB_HOST`, `DB_PORT`, `DB_NAME`, `DB_USER`, `DB_PASSWORD`, và giữ container sống sau khi demo chạy xong để kiểm tra DNS/DB.
- `live-capture`: chỉ chạy khi bật profile `live`, dùng `network_mode: host`, user `0:0`, `NET_ADMIN`, `NET_RAW`, và các biến `CAPTURE_INTERFACE`, `CAPTURE_DURATION`, `CAPTURE_FILTER`.

## Lệnh Kiểm Chứng Local

Build local:

```sh
cmake -S . -B build -DASSET_DISCOVERY_REQUIRE_PCAP=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

Kết quả kiểm chứng ngày 2026-06-29:

```text
100% tests passed, 0 tests failed out of 18
```

Chạy PCAP có filter:

```sh
./build/asset-discovery --pcap samples/arp.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Khi không có cấu hình PostgreSQL trong môi trường hiện tại, output mong đợi có asset từ fixture ARP:

```text
MAC                IPs                     Hostname          First Seen        Last Seen         Sources
--------------------------------------------------------------------------------------------------------
02:42:ac:11:00:02  192.168.1.10                              1699606784.0      1699606784.0      arp
```

Kiểm tra lỗi BPF:

```sh
./build/asset-discovery --pcap samples/arp.pcap --filter invalid --output table
```

Hình dạng lỗi mong đợi:

```text
error: invalid BPF filter for 'samples/arp.pcap': can't parse filter expression: syntax error
```

Lưu ý: binary tự load `.env` từ thư mục hiện tại. Nếu `.env`, `DATABASE_URL`, hoặc biến `PG*` đang trỏ tới PostgreSQL nhưng database chưa chạy, chương trình sẽ cố ghi DB và thoát lỗi trước khi in output.

## Lệnh Docker

Build Docker image:

```sh
docker build -t passive-asset-discovery .
```

Chạy PCAP trong container:

```sh
docker run --rm -v "$PWD/samples:/samples:ro" passive-asset-discovery \
  --pcap /samples/arp.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Chạy PCAP và ghi PostgreSQL bằng Compose:

```sh
docker compose up --build pcap-demo
docker compose exec pcap-demo ping db
docker compose exec db psql -U postgres -d asset_discovery \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets;"
```

Chạy binary local để ghi vào PostgreSQL của Compose:

```sh
docker compose up -d db
./build/asset-discovery --pcap samples/arp.pcap --output json
psql "postgresql://postgres:123456@localhost:5432/asset_discovery" \
  -c "select mac_address, ip_addresses, hostname, first_seen, last_seen, discovery_sources from assets;"
```

Nếu service `db` đã từng tạo volume với credential khác, PostgreSQL giữ credential trong volume cũ. Với dữ liệu demo, có thể xóa volume bằng `docker compose down -v` rồi chạy lại `docker compose up -d db`.

Chạy live capture trong container:

```sh
docker run --rm --user 0:0 --net=host --cap-add=NET_ADMIN --cap-add=NET_RAW \
  passive-asset-discovery \
  --interface eth0 --duration 60 \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Hoặc dùng Compose profile:

```sh
CAPTURE_INTERFACE=eth0 CAPTURE_DURATION=60 docker compose --profile live run --rm live-capture
```

Trong môi trường kiểm chứng hiện tại, chưa chạy được các lệnh Docker vì host không có lệnh `docker`:

```text
/bin/bash: line 1: docker: command not found
```

## Ghi Chú Vận Hành

- PCAP mode không cần quyền mạng đặc biệt; container mặc định chạy bằng user `asset`.
- Live capture thường cần Linux host, `--net=host`, `NET_ADMIN`, `NET_RAW`, và tên interface tồn tại trên host.
- Không dùng privileged mode mặc định vì phạm vi quyền quá rộng cho demo capture.
- Filter BPF được libpcap compile trước khi đọc packet; filter sai cú pháp sẽ trả lỗi khác 0.
- PostgreSQL trong Compose dùng credential demo, không dùng cho môi trường production.
- MAC randomization và MAC spoofing có thể làm một thiết bị xuất hiện như nhiều asset khác nhau.
- Một thiết bị có nhiều interface sẽ được ghi thành nhiều asset nếu các interface dùng MAC khác nhau.
- DHCP metadata phụ thuộc vào traffic quan sát được; thiếu DHCP hostname hoặc IP option thì asset vẫn được ghi nhưng ít metadata hơn.
- Docker live capture khác nhau theo hệ điều hành; Docker Desktop trên macOS/Windows không tương đương Linux host network.

## Kết Quả Mong Đợi

- Docker image build thành công.
- Container đọc được `/samples/arp.pcap` qua volume read-only.
- `pcap-demo` ghi asset vào bảng `assets` trong PostgreSQL service.
- Live capture dừng sau thời lượng đã cấu hình và render cùng asset model như PCAP mode.
