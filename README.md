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

Live capture từ interface trong 60 giây:

```sh
sudo ./build/asset-discovery --interface eth0 --duration 60 \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Live capture không giới hạn thời gian, dừng khi không có packet phù hợp trong 30 giây, đủ 10 asset, hoặc khi nhấn Ctrl+C:

```sh
sudo ./build/asset-discovery --interface eth0 --live \
  --idle-timeout 30 \
  --max-assets 10 \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Trong live infinite mode, `--idle-timeout` tính theo packet được libpcap chấp nhận sau BPF filter; traffic bị filter loại ra sẽ không reset bộ đếm idle. Ctrl+C dừng capture theo hướng graceful, sau đó chương trình vẫn render kết quả cuối và ghi PostgreSQL nếu đã cấu hình.

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

Nếu chỉ muốn xem output trên terminal mà không ghi database, hãy chạy trong thư mục không có `.env` và không đặt `DATABASE_URL` hoặc biến `PG*`.

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
  --interface eth0 --duration 60 \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Live infinite trong Docker:

```sh
docker run --rm --user 0:0 --net=host --cap-add=NET_ADMIN --cap-add=NET_RAW \
  passive-asset-discovery \
  --interface eth0 --live --idle-timeout 30 --max-assets 10 \
  --filter "arp or udp port 67 or udp port 68" \
  --output table
```

Có thể dùng Compose profile cho live capture:

```sh
CAPTURE_INTERFACE=eth0 CAPTURE_DURATION=60 docker compose --profile live run --rm live-capture
```

Compose profile mặc định chạy live timed. Để chạy live infinite:

```sh
CAPTURE_MODE=live CAPTURE_IDLE_TIMEOUT=30 CAPTURE_MAX_ASSETS=10 \
  docker compose --profile live run --rm live-capture
```

Tài liệu Sprint và output contract nằm ở:

- `docs/SPRINT2.md`
- `docs/SPRINT3.md`
- `docs/SPRINT4.md`
- `docs/asset-output-contract.md`
- `docs/system-design.md`
- `docs/build-deploy-demo-guide.md`
- `docs/final-submission-checklist.md`
