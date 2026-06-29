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

Ghi PostgreSQL khi đã có database và `psql` trong `PATH`:

```sh
psql -f db/schema.sql
./build/asset-discovery --pcap samples/arp.pcap --output json
```

Tạo `.env` trong thư mục hiện tại:

```env
PGHOST=localhost
PGPORT=5432
PGDATABASE=asset_discovery
PGUSER=postgres
PGPASSWORD=change-me
```

Sau đó chỉ cần:

```sh
./build/asset-discovery --pcap samples/arp.pcap --output json
```

Tài liệu Sprint 2 và output contract nằm ở:

- `docs/SPRINT2.md`
- `docs/asset-output-contract.md`
