# Phần 5: Demo Đóng Gói Docker Image

Tài liệu này hướng dẫn cách build Docker image của hệ thống và chạy thử nghiệm phân tích các file PCAP offline trong container độc lập.

---

## 1. Build Docker Image

Build image từ `Dockerfile` ở thư mục gốc của repository:

```bash
docker build -t passive-asset-discovery .
```

* **Kỳ vọng:**
  * Build thành công, tạo ra image có tên `passive-asset-discovery`.
  * Dockerfile sử dụng cơ chế **Multi-stage build** để tối ưu hóa kích thước:
    * *Stage Build:* Cài đặt compiler, CMake, và thư viện phát triển `libpcap-dev` để biên dịch binary.
    * *Stage Runtime:* Chỉ cài đặt gói runtime `libpcap0.8`, `postgresql-client` và các thư viện cần thiết, chạy dưới user bảo mật `asset` (non-root).

---

## 2. Phân Tích PCAP Offline Bằng Docker Container

Để container có thể đọc được file PCAP nằm trên host, chúng ta mount thư mục `samples/` của host vào container ở chế độ chỉ đọc (`ro`).

### 2.1. Đọc file PCAP ARP đơn giản (Đầu ra Bảng)
```bash
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  passive-asset-discovery \
  --pcap /samples/arp.pcap \
  --output table
```
* **Kỳ vọng:** Output in ra bảng kết quả phân tích tương tự khi chạy native.

### 2.2. Đọc file PCAP đa thiết bị (Đầu ra JSON)
```bash
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  passive-asset-discovery \
  --pcap /samples/multi-asset.pcap \
  --output json
```

### 2.3. Đọc file PCAP áp dụng bộ lọc BPF
```bash
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  passive-asset-discovery \
  --pcap /samples/multi-asset.pcap \
  --filter "arp" \
  --output table
```

### 2.4. Đọc file PCAP (Đầu ra CSV)
```bash
docker run --rm \
  -v "$PWD/samples:/samples:ro" \
  passive-asset-discovery \
  --pcap /samples/multi-asset.pcap \
  --output csv
```

### 2.5. Xem trợ giúp (Help text) trong Docker
```bash
docker run --rm passive-asset-discovery --help
```

---

## 3. Phân Tích PCAP Và Ghi Nhận PostgreSQL Từ Docker

Để container chạy ứng dụng có thể kết nối với PostgreSQL đang chạy trên host/container khác, chúng cần chung mạng Docker.

Giả sử PostgreSQL đang chạy trong mạng Docker Compose `asset-net` (được tạo tự động bởi Compose hoặc tạo thủ công):

```bash
# Đảm bảo database đang chạy
docker compose up -d db

# Chạy container app trong cùng mạng của Compose (thông thường là <folder_name>_default)
# Truyền thông tin kết nối qua các biến môi trường DB_*
docker run --rm \
  --network passive-network-asset-discovery-system_default \
  -v "$PWD/samples:/samples:ro" \
  -e DB_HOST=db \
  -e DB_PORT=5432 \
  -e DB_NAME=asset_discovery \
  -e DB_USER=postgres \
  -e DB_PASSWORD=123456 \
  passive-asset-discovery \
  --pcap /samples/multi-asset.pcap \
  --filter "arp or udp port 67 or udp port 68" \
  --output json

# Kiểm tra dữ liệu trong DB
docker compose exec db psql -U postgres -d asset_discovery \
  -c "select mac_address, ip_addresses, hostname from assets order by mac_address;"
```
* **Kỳ vọng:** Dữ liệu phân tích từ container app được ghi nhận chính xác vào PostgreSQL.
